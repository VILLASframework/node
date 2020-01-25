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
#include <villas/utils.hpp>
#include <villas/list.h>
#include <villas/hook_list.hpp>
#include <villas/advio.h>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/config_helper.hpp>
#include <villas/log.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/kernel/if.h>

#ifdef WITH_NETEM
  #include <villas/kernel/nl.h>
#endif

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

SuperNode::SuperNode() :
	state(State::INITIALIZED),
	idleStop(false),
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
	hugepages(DEFAULT_NR_HUGEPAGES)
{
	nodes.state = State::DESTROYED;
	paths.state = State::DESTROYED;
	interfaces.state = State::DESTROYED;

	vlist_init(&nodes);
	vlist_init(&paths);
	vlist_init(&interfaces);

#ifdef WITH_NETEM
	nl_init(); /* Fill link cache */
#endif /* WITH_NETEM */

	char hname[128];
	gethostname(hname, 128);

	name = hname;

	logger = logging.get("super_node");
}

void SuperNode::parse(const std::string &u)
{
	config.load(u);

	parse(config.root);
}

void SuperNode::parse(json_t *cfg)
{
	int ret;
	const char *nme = nullptr;

	assert(state != State::STARTED);

	json_t *json_nodes = nullptr;
	json_t *json_paths = nullptr;
	json_t *json_logging = nullptr;
	json_t *json_web = nullptr;
	json_t *json_ethercat = nullptr;

	json_error_t err;

	idleStop = true;

	ret = json_unpack_ex(cfg, &err, JSON_STRICT, "{ s?: o, s?: o, s?: o, s?: o, s?: o, s?: i, s?: i, s?: i, s?: s, s?: b }",
		"http", &json_web,
		"ethercat", &json_ethercat,
		"logging", &json_logging,
		"nodes", &json_nodes,
		"paths", &json_paths,
		"hugepages", &hugepages,
		"affinity", &affinity,
		"priority", &priority,
		"name", &nme,
		"idle_stop", &idleStop
	);
	if (ret)
		throw ConfigError(cfg, err, "node-config");

	if (nme)
		name = nme;

#ifdef WITH_WEB
	if (json_web)
		web.parse(json_web);
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
			struct node_type *nt;
			const char *type;

			ret = node_is_valid_name(name);
			if (!ret)
				throw RuntimeError("Invalid name for node: {}", name);

			ret = json_unpack_ex(json_node, &err, 0, "{ s: s }", "type", &type);
			if (ret)
				throw ConfigError(cfg, err, "node-config-node-type", "Failed to parse type of node '{}'", name);

			nt = node_type_lookup(type);
			if (!nt)
				throw ConfigError(json_node, "node-config-node-type", "Invalid node type: {}", type);

			auto *n = new struct node;
			if (!n)
				throw RuntimeError("Failed to allocate memory");

			n->state = State::DESTROYED;
			n->in.state = State::DESTROYED;
			n->out.state = State::DESTROYED;

			ret = node_init(n, nt);
			if (ret)
				throw RuntimeError("Failed to initialize node");

			ret = node_parse(n, json_node, name);
			if (ret) {
				auto config_id = fmt::format("node-config-node-{}", type);

				throw ConfigError(json_node, config_id, "Failed to parse configuration of node '{}'", name);
			}

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
parse:			path *p = new path;

			ret = path_init(p);
			if (ret)
				throw RuntimeError("Failed to initialize path");

			ret = path_parse(p, json_path, &nodes);
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
		auto *n = (struct node *) vlist_at(&nodes, i);

		ret = node_check(n);
		if (ret)
			throw RuntimeError("Invalid configuration for node {}", node_name(n));
	}

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct path *) vlist_at(&paths, i);

		ret = path_check(p);
		if (ret)
			throw RuntimeError("Invalid configuration for path {}", path_name(p));
	}

	state = State::CHECKED;
}

void SuperNode::startNodeTypes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct node *) vlist_at(&nodes, i);

		ret = node_type_start(n->_vt, this);
		if (ret)
			throw RuntimeError("Failed to start node-type: {}", node_type_name(n->_vt));
	}
}

void SuperNode::startInterfaces()
{
#ifdef WITH_NETEM
	int ret;

	for (size_t i = 0; i < vlist_length(&interfaces); i++) {
		auto *j = (struct interface *) vlist_at(&interfaces, i);

		ret = if_start(j);
		if (ret)
			throw RuntimeError("Failed to initialize network interface: {}", if_name(j));
	}
#endif /* WITH_NETEM */
}

void SuperNode::startNodes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct node *) vlist_at(&nodes, i);

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
		auto *p = (struct path *) vlist_at(&paths, i);

		if (!path_is_enabled(p))
			continue;

		ret = path_start(p);
		if (ret)
			throw RuntimeError("Failed to start path: {}", path_name(p));
	}
}

void SuperNode::prepareNodes()
{
	int ret, refs;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct node *) vlist_at(&nodes, i);

		refs = vlist_count(&paths, (cmp_cb_t) path_uses_node, n);
		if (refs <= 0) {
			logger->warn("No path is using the node {}. Skipping...", node_name(n));
			n->enabled = false;
		}

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
		auto *p = (struct path *) vlist_at(&paths, i);

		if (!path_is_enabled(p))
			continue;

		ret = path_prepare(p);
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

	state = State::PREPARED;
}

void SuperNode::start()
{
	int ret;

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

	ret = task_init(&task, 1.0, CLOCK_REALTIME);
	if (ret)
		throw RuntimeError("Failed to create timer");

	Stats::printHeader(Stats::Format::HUMAN);

	state = State::STARTED;
}

void SuperNode::stopPaths()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct path *) vlist_at(&paths, i);

		ret = path_stop(p);
		if (ret)
			throw RuntimeError("Failed to stop path: {}", path_name(p));
	}
}

void SuperNode::stopNodes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct node *) vlist_at(&nodes, i);

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

	for (size_t j = 0; j < vlist_length(&interfaces); j++) {
		struct interface *i = (struct interface *) vlist_at(&interfaces, j);

		ret = if_stop(i);
		if (ret)
			throw RuntimeError("Failed to stop interface: {}", if_name(i));
	}
#endif /* WITH_NETEM */
}

void SuperNode::stop()
{
	int ret;

	ret = task_destroy(&task);
	if (ret)
		throw RuntimeError("Failed to stop timer");

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
		task_wait(&task);

		ret = periodic();
		if (ret)
			state = State::STOPPING;
	}
}

SuperNode::~SuperNode()
{
	assert(state != State::STARTED);

	vlist_destroy(&paths,      (dtor_cb_t) path_destroy, true);
	vlist_destroy(&nodes,      (dtor_cb_t) node_destroy, true);
	vlist_destroy(&interfaces, (dtor_cb_t) if_destroy, true);
}

int SuperNode::periodic()
{
	int started = 0;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct path *) vlist_at(&paths, i);

		if (p->state == State::STARTED) {
			started++;

#ifdef WITH_HOOKS
			hook_list_periodic(&p->hooks);
#endif /* WITH_HOOKS */
		}
	}

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct node *) vlist_at(&nodes, i);

		if (n->state == State::STARTED) {
#ifdef WITH_HOOKS
			hook_list_periodic(&n->in.hooks);
			hook_list_periodic(&n->out.hooks);
#endif /* WITH_HOOKS */
		}
	}

	if (idleStop && state == State::STARTED && started == 0) {
		info("No more active paths. Stopping super-node");

		return -1;
	}

	return 0;
}
