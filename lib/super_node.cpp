/** The super node object holding the state of the application.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <cstdlib>
#include <cstring>

#include <villas/super_node.hpp>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/path_source.h>
#include <villas/path_destination.h>
#include <villas/utils.hpp>
#include <villas/list.h>
#include <villas/hook_list.hpp>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/config_helper.hpp>
#include <villas/log.hpp>
#include <villas/timing.h>
#include <villas/node/exceptions.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/kernel/if.hpp>

#ifdef WITH_NETEM
  #include <villas/kernel/nl.hpp>
#endif

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

typedef char uuid_string_t[37];

SuperNode::SuperNode() :
	state(State::INITIALIZED),
	idleStop(-1),
#ifdef WITH_API
	api(this),
#endif
#ifdef WITH_WEB
  #ifdef WITH_API
	web(&api),
  #else
	web(),
  #endif
#endif
	priority(0),
	affinity(0),
	hugepages(DEFAULT_NR_HUGEPAGES),
	statsRate(1.0),
	task(CLOCK_REALTIME),
	started(time_now())
{
	int ret;

	char hname[128];
	ret = gethostname(hname, sizeof(hname));
	if (ret)
		throw SystemError("Failed to determine hostname");

	/* Default UUID is derived from hostname */
	uuid_generate_from_str(uuid, hname);

	ret = vlist_init(&nodes);
	if (ret)
		throw RuntimeError("Failed to initialize list");

	ret = vlist_init(&paths);
	if (ret)
		throw RuntimeError("Failed to initialize list");

#ifdef WITH_NETEM
	kernel::nl::init(); /* Fill link cache */
#endif /* WITH_NETEM */

	logger = logging.get("super_node");
}

void SuperNode::parse(const std::string &u)
{
	config.root = config.load(u);

	parse(config.root);
}

void SuperNode::parse(json_t *root)
{
	int ret;

	assert(state != State::STARTED);

	const char *uuid_str = nullptr;

	json_t *json_nodes = nullptr;
	json_t *json_paths = nullptr;
	json_t *json_logging = nullptr;
	json_t *json_http = nullptr;

	json_error_t err;

	idleStop = 1;

	ret = json_unpack_ex(root, &err, 0, "{ s?: F, s?: o, s?: o, s?: o, s?: o, s?: i, s?: i, s?: i, s?: b, s?: s }",
		"stats", &statsRate,
		"http", &json_http,
		"logging", &json_logging,
		"nodes", &json_nodes,
		"paths", &json_paths,
		"hugepages", &hugepages,
		"affinity", &affinity,
		"priority", &priority,
		"idle_stop", &idleStop,
		"uuid", &uuid_str
	);
	if (ret)
		throw ConfigError(root, err, "node-config", "Unpacking top-level config failed");

	if (uuid_str) {
		ret = uuid_parse(uuid_str, uuid);
		if (ret)
			throw ConfigError(root, "node-config-uuid", "Failed to parse UUID: {}", uuid_str);
	}

#ifdef WITH_WEB
	if (json_http)
		web.parse(json_http);
#endif /* WITH_WEB */

	if (json_logging)
		logging.parse(json_logging);

	/* Parse nodes */
	if (json_nodes) {
		if (!json_is_object(json_nodes))
			throw ConfigError(json_nodes, "node-config-nodes", "Setting 'nodes' must be a group with node name => group mappings.");

		const char *name;
		json_t *json_node;
		json_object_foreach(json_nodes, name, json_node) {
			struct vnode_type *nt;
			const char *type;

			ret = node_is_valid_name(name);
			if (!ret)
				throw RuntimeError("Invalid name for node: {}", name);

			ret = json_unpack_ex(json_node, &err, 0, "{ s: s }", "type", &type);
			if (ret)
				throw ConfigError(root, err, "node-config-node-type", "Failed to parse type of node '{}'", name);

			json_object_set(json_node, "name", json_string(name));

			nt = node_type_lookup(type);
			if (!nt)
				throw ConfigError(json_node, "node-config-node-type", "Invalid node type: {}", type);

			auto *n = new struct vnode;
			if (!n)
				throw MemoryAllocationError();

			ret = node_init(n, nt);
			if (ret)
				throw RuntimeError("Failed to initialize node");

			ret = node_parse(n, json_node, uuid);
			if (ret) {
				auto config_id = fmt::format("node-config-node-{}", type);

				throw ConfigError(json_node, config_id, "Failed to parse configuration of node '{}'", name);
			}

			json_object_del(json_node, "name");

			vlist_push(&nodes, n);
		}
	}

	/* Parse paths */
	if (json_paths) {
		if (!json_is_array(json_paths))
			logger->warn("Setting 'paths' must be a list of objects");

		size_t i;
		json_t *json_path;
		json_array_foreach(json_paths, i, json_path) {
parse:			auto *p = new vpath;
			if (!p)
				throw MemoryAllocationError();

			ret = path_init(p);
			if (ret)
				throw RuntimeError("Failed to initialize path");

			ret = path_parse(p, json_path, &nodes, uuid);
			if (ret)
				throw RuntimeError("Failed to parse path");

			vlist_push(&paths, p);

			if (p->reverse) {
				/* Only simple paths can be reversed */
				ret = path_is_simple(p);
				if (!ret)
					throw RuntimeError("Complex paths can not be reversed!");

				/* Parse a second time with in/out reversed */
				json_path = json_copy(json_path);

				json_t *json_in = json_object_get(json_path, "in");
				json_t *json_out = json_object_get(json_path, "out");

				if (json_equal(json_in, json_out))
					throw RuntimeError("Can not reverse path with identical in/out nodes!");

				json_object_set(json_path, "reverse", json_false());
				json_object_set(json_path, "in", json_out);
				json_object_set(json_path, "out", json_in);

				goto parse;
			}
		}
	}

	state = State::PARSED;
}

void SuperNode::check()
{
	int ret;

	assert(state == State::INITIALIZED || state == State::PARSED || state == State::CHECKED);

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);

		ret = node_check(n);
		if (ret)
			throw RuntimeError("Invalid configuration for node {}", node_name(n));
	}

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct vpath *) vlist_at(&paths, i);

		path_check(p);
	}

	state = State::CHECKED;
}

void SuperNode::startNodeTypes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);

		ret = node_type_start(n->_vt, this);
		if (ret)
			throw RuntimeError("Failed to start node-type: {}", node_type_name(n->_vt));
	}
}

void SuperNode::startInterfaces()
{
#ifdef WITH_NETEM
	int ret;

	for (auto *i : interfaces) {
		ret = i->start();
		if (ret)
			throw RuntimeError("Failed to start network interface: {}", i->getName());
	}
#endif /* WITH_NETEM */
}

void SuperNode::startNodes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);

		if (!node_is_enabled(n))
			continue;

		ret = node_start(n);
		if (ret)
			throw RuntimeError("Failed to start node: {}", node_name(n));
	}
}

void SuperNode::startPaths()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct vpath *) vlist_at(&paths, i);

		if (!path_is_enabled(p))
			continue;

		ret = path_start(p);
		if (ret)
			throw RuntimeError("Failed to start path: {}", path_name(p));
	}
}

void SuperNode::prepareNodes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);

		if (!node_is_enabled(n))
			continue;

		ret = node_prepare(n);
		if (ret)
			throw RuntimeError("Failed to prepare node: {}", node_name(n));
	}
}

void SuperNode::preparePaths()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct vpath *) vlist_at(&paths, i);

		if (!path_is_enabled(p))
			continue;

		ret = path_prepare(p, &nodes);
		if (ret)
			throw RuntimeError("Failed to prepare path: {}", path_name(p));
	}
}

void SuperNode::prepare()
{
	int ret;

	assert(state == State::CHECKED);

	ret = memory_init(hugepages);
	if (ret)
		throw RuntimeError("Failed to initialize memory system");

	kernel::rt::init(priority, affinity);

	prepareNodes();
	preparePaths();

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);
		if (vlist_length(&n->sources) == 0 &&
		    vlist_length(&n->destinations) == 0) {
			logger->info("Node {} is not used by any path. Disabling...", node_name(n));
			n->enabled = false;
		}
	}

	state = State::PREPARED;
}

void SuperNode::start()
{
	assert(state == State::PREPARED);

#ifdef WITH_API
	api.start();
#endif

#ifdef WITH_WEB
	web.start();
#endif

	startNodeTypes();
	startInterfaces();
	startNodes();
	startPaths();

	if (statsRate > 0) // A rate <0 will disable the periodic stats
		task.setRate(statsRate);

	Stats::printHeader(Stats::Format::HUMAN);

	state = State::STARTED;
}

void SuperNode::stopPaths()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct vpath *) vlist_at(&paths, i);

		ret = path_stop(p);
		if (ret)
			throw RuntimeError("Failed to stop path: {}", path_name(p));
	}
}

void SuperNode::stopNodes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);

		ret = node_stop(n);
		if (ret)
			throw RuntimeError("Failed to stop node: {}", node_name(n));
	}
}

void SuperNode::stopNodeTypes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&plugins); i++) {
		auto *p = (struct plugin *) vlist_at(&plugins, i);

		if (p->type == PluginType::NODE) {
			ret = node_type_stop(&p->node);
			if (ret)
				throw RuntimeError("Failed to stop node-type: {}", node_type_name(&p->node));
		}
	}
}

void SuperNode::stopInterfaces()
{
#ifdef WITH_NETEM
	int ret;

	for (auto *i : interfaces) {
		ret = i->stop();
		if (ret)
			throw RuntimeError("Failed to stop interface: {}", i->getName());
	}
#endif /* WITH_NETEM */
}

void SuperNode::stop()
{
	stopNodes();
	stopPaths();
	stopNodeTypes();
	stopInterfaces();

#ifdef WITH_API
	api.stop();
#endif

#ifdef WITH_WEB
	web.stop();
#endif

	state = State::STOPPED;
}

void SuperNode::run()
{
	int ret;

	while (state == State::STARTED) {
		task.wait();

		ret = periodic();
		if (ret)
			state = State::STOPPING;
	}
}

SuperNode::~SuperNode()
{
	int ret __attribute__((unused));

	assert(state != State::STARTED);

	ret = vlist_destroy(&paths,      (dtor_cb_t) path_destroy, true);
	ret = vlist_destroy(&nodes,      (dtor_cb_t) node_destroy, true);
}

int SuperNode::periodic()
{
	int started = 0;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct vpath *) vlist_at(&paths, i);

		if (p->state == State::STARTED) {
			started++;

#ifdef WITH_HOOKS
			hook_list_periodic(&p->hooks);
#endif /* WITH_HOOKS */
		}
	}

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);

		if (n->state == State::STARTED) {
#ifdef WITH_HOOKS
			hook_list_periodic(&n->in.hooks);
			hook_list_periodic(&n->out.hooks);
#endif /* WITH_HOOKS */
		}
	}

	if (idleStop > 0 && state == State::STARTED && started == 0) {
		logger->info("No more active paths. Stopping super-node");

		return -1;
	}

	return 0;
}

#ifdef WITH_GRAPHVIZ
static void
set_attr(void *ptr, const std::string &key, const std::string &value, bool html = false) {
	Agraph_t *g = agraphof(ptr);

	char *d = (char *) "";
	char *k = (char *) key.c_str();
	char *v = (char *) value.c_str();
	char *vd = html ? agstrdup_html(g, v) : agstrdup(g, v);

	agsafeset(ptr, k, vd, d);
}


graph_t * SuperNode::getGraph()
{
	Agraph_t *g;
	Agnode_t *m;

	g = agopen((char *) "g", Agdirected, 0);

	std::map<struct vnode *, Agnode_t *> nodeMap;

	uuid_string_t uuid_str;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct vnode *) vlist_at(&nodes, i);

		nodeMap[n] = agnode(g, (char *) node_name_short(n), 1);

		uuid_unparse(n->uuid, uuid_str);

		set_attr(nodeMap[n], "shape", "ellipse");
		set_attr(nodeMap[n], "tooltip", fmt::format("type={}, uuid={}", plugin_name(node_type(n)), uuid_str));
		// set_attr(nodeMap[n], "fixedsize", "true");
		// set_attr(nodeMap[n], "width", "0.15");
		// set_attr(nodeMap[n], "height", "0.15");
	}

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct vpath *) vlist_at(&paths, i);

		auto name = fmt::format("path_{}", i);

		m = agnode(g, (char *) name.c_str(), 1);

		uuid_unparse(p->uuid, uuid_str);

		set_attr(m, "shape", "box");
		set_attr(m, "tooltip", fmt::format("uuid={}", uuid_str));

		for (size_t j = 0; j < vlist_length(&p->sources); j++) {
			auto *ps = (struct vpath_source *) vlist_at(&p->sources, j);

			agedge(g, nodeMap[ps->node], m, nullptr, 1);
		}

		for (size_t j = 0; j < vlist_length(&p->destinations); j++) {
			auto *pd = (struct vpath_destination *) vlist_at(&p->destinations, j);

			agedge(g, m, nodeMap[pd->node], nullptr, 1);
		}
	}

	return g;

}
#endif /* WITH_GRAPHVIZ */
