/** The super node object holding the state of the application.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include <villas/super_node.hpp>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/utils.h>
#include <villas/list.h>
#include <villas/hook.h>
#include <villas/advio.h>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/config_helper.h>
#include <villas/log.hpp>
#include <villas/exceptions.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/kernel/if.h>

#ifdef WITH_NETEM
  #include <villas/kernel/nl.h>
#endif

using namespace villas;
using namespace villas::node;

SuperNode::SuperNode() :
	state(STATE_INITIALIZED),
	idleStop(false),
	priority(0),
	affinity(0),
	hugepages(DEFAULT_NR_HUGEPAGES),
#ifdef WITH_API
	api(this),
#ifdef WITH_WEB
	web(&api),
#endif
#endif
	json(nullptr)
{
	nodes.state = STATE_DESTROYED;
	paths.state = STATE_DESTROYED;
	interfaces.state = STATE_DESTROYED;

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

int SuperNode::parseUri(const std::string &u)
{
	json_error_t err;

	FILE *f;
	AFILE *af;

	/* Via stdin */
	if (u == "-") {
		logger->info("Reading configuration from standard input");

		af = nullptr;
		f = stdin;
	}
	else {
		logger->info("Reading configuration from URI: {}", u);

		af = afopen(u.c_str(), "r");
		if (!af)
			throw RuntimeError("Failed to open configuration from: {}", u);

		f = af->file;
	}

	/* Parse config */
	json = json_loadf(f, 0, &err);
	if (json == nullptr) {
#ifdef LIBCONFIG_FOUND
		int ret;

		config_t cfg;
		config_setting_t *json_root = nullptr;

		config_init(&cfg);
		config_set_auto_convert(&cfg, 1);

		/* Setup libconfig include path.
		 * This is only supported for local files */
		if (access(u.c_str(), F_OK) != -1) {
			char *cpy = strdup(u.c_str());

			config_set_include_dir(&cfg, dirname(cpy));

			free(cpy);
		}

		if (af)
			arewind(af);
		else
			rewind(f);

		ret = config_read(&cfg, f);
		if (ret != CONFIG_TRUE) {
			logger->warn("conf: {} in {}:{}", config_error_text(&cfg), u.c_str(), config_error_line(&cfg));
			logger->warn("json: {} in {}:{}:{}", err.text, err.source, err.line, err.column);
			logger->error("Failed to parse configuration");
			killme(SIGABRT);
		}

		json_root = config_root_setting(&cfg);

		json = config_to_json(json_root);
		if (json == nullptr)
			throw RuntimeError("Failed to convert JSON to configuration file");

		config_destroy(&cfg);
#else
		throw JsonError(err, "Failed to parse configuration file");
#endif /* LIBCONFIG_FOUND */
		}

	/* Close configuration file */
	if (af)
		afclose(af);
	else if (f != stdin)
		fclose(f);

	uri = u;

	return parseJson(json);

	return 0;
}

int SuperNode::parseJson(json_t *j)
{
	int ret;
	const char *nme = nullptr;

	assert(state != STATE_STARTED);

	json_t *json_nodes = nullptr;
	json_t *json_paths = nullptr;
	json_t *json_logging = nullptr;
	json_t *json_web = nullptr;

	json_error_t err;

	idleStop = true;

	ret = json_unpack_ex(j, &err, 0, "{ s?: o, s?: o, s?: o, s?: o, s?: i, s?: i, s?: i, s?: s, s?: b }",
		"http", &json_web,
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
		throw JsonError(err, "Failed to parse global configuration");

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
			if (ret)
				throw RuntimeError("Invalid name for node: {}", name);

			ret = json_unpack_ex(json_node, &err, 0, "{ s: s }", "type", &type);
			if (ret)
				throw JsonError(err, "Failed to parse node");

			nt = node_type_lookup(type);
			if (!nt)
				throw RuntimeError("Invalid node type: {}", type);

			auto *n = (struct node *) alloc(sizeof(struct node));

			ret = node_init(n, nt);
			if (ret)
				throw RuntimeError("Failed to initialize node");

			ret = node_parse(n, json_node, name);
			if (ret)
				throw RuntimeError("Failed to parse node");

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
parse:			path *p = (path *) alloc(sizeof(path));

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
				if (ret)
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

	json = j;

	state = STATE_PARSED;

	return 0;
}

int SuperNode::check()
{
	int ret;

	assert(state == STATE_INITIALIZED || state == STATE_PARSED || state == STATE_CHECKED);

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

	state = STATE_CHECKED;

	return 0;
}

void SuperNode::startNodeTypes()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct node *) vlist_at(&nodes, i);

		ret = node_type_start(n->_vt, reinterpret_cast<super_node *>(this));
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

		ret = node_init2(n);
		if (ret)
			throw RuntimeError("Failed to prepare node: {}", node_name(n));

		int refs = vlist_count(&paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0) {
			ret = node_start(n);
			if (ret)
				throw RuntimeError("Failed to start node: {}", node_name(n));
		}
		else
			logger->warn("No path is using the node {}. Skipping...", node_name(n));
	}
}

void SuperNode::startPaths()
{
	int ret;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct path *) vlist_at(&paths, i);

		if (p->enabled) {
			ret = path_init2(p);
			if (ret)
				throw RuntimeError("Failed to prepare path: {}", path_name(p));

			ret = path_start(p);
			if (ret)
				throw RuntimeError("Failed to start path: {}", path_name(p));
		}
		else
			logger->warn("Path {} is disabled. Skipping...", path_name(p));
	}
}

void SuperNode::start()
{
	int ret;

	assert(state == STATE_CHECKED);

	ret = memory_init(hugepages);
	if (ret)
		throw RuntimeError("Failed to initialize memory system");

	kernel::rt::init(priority, affinity);

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

	stats_print_header(STATS_FORMAT_HUMAN);

	state = STATE_STARTED;
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

		if (p->type == PLUGIN_TYPE_NODE) {
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

	stopPaths();
	stopNodes();
	stopNodeTypes();
	stopInterfaces();

#ifdef WITH_API
	api.stop();
#endif
#ifdef WITH_WEB
	web.stop();
#endif

	state = STATE_STOPPED;
}

void SuperNode::run()
{
	int ret;

	while (state == STATE_STARTED) {
		task_wait(&task);

		ret = periodic();
		if (ret)
			state = STATE_STOPPING;
	}
}

SuperNode::~SuperNode()
{
	assert(state != STATE_STARTED);

	vlist_destroy(&paths,      (dtor_cb_t) path_destroy, true);
	vlist_destroy(&nodes,      (dtor_cb_t) node_destroy, true);
	vlist_destroy(&interfaces, (dtor_cb_t) if_destroy, true);

	if (json)
		json_decref(json);
}

int SuperNode::periodic()
{
	int ret;

	int started = 0;

	for (size_t i = 0; i < vlist_length(&paths); i++) {
		auto *p = (struct path *) vlist_at(&paths, i);

		if (p->state == STATE_STARTED) {
			started++;

#ifdef WITH_HOOKS
			for (size_t j = 0; j < vlist_length(&p->hooks); j++) {
				hook *h = (struct hook *) vlist_at(&p->hooks, j);

				ret = hook_periodic(h);
				if (ret)
					return ret;
			}
#endif /* WITH_HOOKS */
		}
	}

	for (size_t i = 0; i < vlist_length(&nodes); i++) {
		auto *n = (struct node *) vlist_at(&nodes, i);

		if (n->state != STATE_STARTED)
			continue;

#ifdef WITH_HOOKS
		for (size_t j = 0; j < vlist_length(&n->in.hooks); j++) {
			auto *h = (struct hook *) vlist_at(&n->in.hooks, j);

			ret = hook_periodic(h);
			if (ret)
				return ret;
		}

		for (size_t j = 0; j < vlist_length(&n->out.hooks); j++) {
			auto *h = (struct hook *) vlist_at(&n->out.hooks, j);

			ret = hook_periodic(h);
			if (ret)
				return ret;
		}
#endif /* WITH_HOOKS */
	}

	if (idleStop && state == STATE_STARTED && started == 0) {
		info("No more active paths. Stopping super-node");

		return -1;
	}

	return 0;
}


/* C-compatability */
extern "C" {
	struct vlist * super_node_get_nodes(struct super_node *sn)
	{
		SuperNode *ssn = reinterpret_cast<SuperNode *>(sn);

		return ssn->getNodes();
	}

	struct vlist * super_node_get_paths(struct super_node *sn)
	{
		SuperNode *ssn = reinterpret_cast<SuperNode *>(sn);

		return ssn->getPaths();
	}

	struct vlist * super_node_get_interfaces(struct super_node *sn)
	{
		SuperNode *ssn = reinterpret_cast<SuperNode *>(sn);

		return ssn->getInterfaces();
	}
#ifdef WITH_WEB
	struct web * super_node_get_web(struct super_node *sn)
	{
		SuperNode *ssn = reinterpret_cast<SuperNode *>(sn);
		Web *w = ssn->getWeb();

		return reinterpret_cast<web *>(w);
	}
#endif
	struct lws_context * web_get_context(struct web *w)
	{
		Web *ws = reinterpret_cast<Web *>(w);

		return ws->getContext();
	}

	struct lws_vhost * web_get_vhost(struct web *w)
	{
		Web *ws = reinterpret_cast<Web *>(w);

		return ws->getVHost();
	}

	enum state web_get_state(struct web *w)
	{
		Web *ws = reinterpret_cast<Web *>(w);

		return ws->getState();
	}

#ifdef WITH_WEB
	int web_callback_on_writable(struct web *w, struct lws *wsi)
	{
		return lws_callback_on_writable(wsi);
	}
#endif
}
