/** The super node object holding the state of the application.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

using namespace villas;
using namespace villas::node;

Logger SuperNode::logger = logging.get("super_node");

SuperNode::SuperNode() :
	state(STATE_INITIALIZED),
	priority(0),
	affinity(0),
	hugepages(DEFAULT_NR_HUGEPAGES),
	stats(0),
	api(this),
	web(&api),
	json(nullptr)
{
	list_init(&nodes);
	list_init(&paths);
	list_init(&plugins);

	char hname[128];
	gethostname(hname, 128);

	name = hname;
}

int SuperNode::parseUri(const std::string &u)
{
	json_error_t err;

	logger->info("Parsing configuration");

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
			throw new RuntimeError("Failed to open configuration from: {}", u);

		f = af->file;
	}

	/* Parse config */
	json = json_loadf(f, 0, &err);
	if (json == nullptr) {
#ifdef LIBCONFIG_FOUND
		int ret;

		config_t cfg;
		config_setting_t *json_root = nullptr;

		logger->warn("Failed to parse JSON configuration. Re-trying with old libconfig format.");
		logger->warn("  Please consider migrating to the new format using the 'conf2json' command.");

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
			throw new RuntimeError("Failed to convert JSON to configuration file");

		config_destroy(&cfg);
#else
		throw new JsonError(err, "Failed to parse configuration file");
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
	json_t *json_plugins = nullptr;
	json_t *json_logging = nullptr;
	json_t *json_web = nullptr;

	json_error_t err;

	ret = json_unpack_ex(j, &err, 0, "{ s?: o, s?: o, s?: o, s?: o, s?: o, s?: i, s?: i, s?: i, s?: F, s?: s }",
		"http", &json_web,
		"logging", &json_logging,
		"plugins", &json_plugins,
		"nodes", &json_nodes,
		"paths", &json_paths,
		"hugepages", &hugepages,
		"affinity", &affinity,
		"priority", &priority,
		"stats", &stats,
		"name", &nme
	);
	if (ret)
		throw new JsonError(err, "Failed to parse global configuration");

	if (nme)
		name = nme;

#ifdef WITH_WEB
	if (json_web)
		web.parse(json_web);
#endif /* WITH_WEB */

	if (json_logging)
		logging.parse(json_logging);

	/* Parse plugins */
	if (json_plugins) {
		if (!json_is_array(json_plugins))
			throw new ConfigError(json_plugins, "node-config-plugins", "Setting 'plugins' must be a list of strings");

		size_t i;
		json_t *json_plugin;
		json_array_foreach(json_plugins, i, json_plugin) {
			auto *p = (plugin *) alloc(sizeof(plugin));

			ret = plugin_init(p);
			if (ret)
				throw new RuntimeError("Failed to initialize plugin");

			ret = plugin_parse(p, json_plugin);
			if (ret)
				throw new RuntimeError("Failed to parse plugin");

			list_push(&plugins, p);
		}
	}

	/* Parse nodes */
	if (json_nodes) {
		if (!json_is_object(json_nodes))
			throw new ConfigError(json_nodes, "node-config-nodes", "Setting 'nodes' must be a group with node name => group mappings.");

		const char *name;
		json_t *json_node;
		json_object_foreach(json_nodes, name, json_node) {
			struct node_type *nt;
			const char *type;

			ret = json_unpack_ex(json_node, &err, 0, "{ s: s }", "type", &type);
			if (ret)
				throw new JsonError(err, "Failed to parse node");

			nt = node_type_lookup(type);
			if (!nt)
				throw new RuntimeError("Invalid node type: {}", type);

			auto *n = (struct node *) alloc(sizeof(struct node));

			ret = node_init(n, nt);
			if (ret)
				throw new RuntimeError("Failed to initialize node");

			ret = node_parse(n, json_node, name);
			if (ret)
				throw new RuntimeError("Failed to parse node");

			list_push(&nodes, n);
		}
	}

	/* Parse paths */
	if (json_paths) {
		if (!json_is_array(json_paths))
			logger->warn("Setting 'paths' must be a list of objects");

		size_t i;
		json_t *json_path;
		json_array_foreach(json_paths, i, json_path) {
			path *p = (path *) alloc(sizeof(path));

			ret = path_init(p);
			if (ret)
				throw new RuntimeError("Failed to initialize path");

			ret = path_parse(p, json_path, &nodes);
			if (ret)
				throw new RuntimeError("Failed to parse path");

			list_push(&paths, p);

			if (p->reverse) {
				path *r = (path *) alloc(sizeof(path));

				ret = path_init(r);
				if (ret)
					throw new RuntimeError("Failed to init path");

				ret = path_reverse(p, r);
				if (ret)
					throw new RuntimeError("Failed to reverse path {}", path_name(p));

				list_push(&paths, r);
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

	for (size_t i = 0; i < list_length(&nodes); i++) {
		auto *n = (struct node *) list_at(&nodes, i);

		ret = node_check(n);
		if (ret)
			throw new RuntimeError("Invalid configuration for node {}", node_name(n));
	}

	for (size_t i = 0; i < list_length(&paths); i++) {
		auto *p = (struct path *) list_at(&paths, i);

		ret = path_check(p);
		if (ret)
			throw new RuntimeError("Invalid configuration for path {}", path_name(p));
	}

	state = STATE_CHECKED;

	return 0;
}

int SuperNode::start()
{
	int ret;

	assert(state == STATE_CHECKED);

	memory_init(hugepages);
	kernel::rt::init(priority, affinity);

#ifdef WITH_API
	api.start();
#endif

#ifdef WITH_WEB
	web.start();
#endif

	logger->info("Starting node-types");
	for (size_t i = 0; i < list_length(&nodes); i++) {
		auto *n = (struct node *) list_at(&nodes, i);

		ret = node_type_start(n->_vt);//, this); // @todo: port to C++
		if (ret)
			throw new RuntimeError("Failed to start node-type: {}", node_type_name(n->_vt));
	}

	logger->info("Starting nodes");
	for (size_t i = 0; i < list_length(&nodes); i++) {
		auto *n = (struct node *) list_at(&nodes, i);

		ret = node_init2(n);
		if (ret)
			throw new RuntimeError("Failed to prepare node: {}", node_name(n));

		int refs = list_count(&paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0) {
			ret = node_start(n);
			if (ret)
				throw new RuntimeError("Failed to start node: {}", node_name(n));
		}
		else
			logger->warn("No path is using the node {}. Skipping...", node_name(n));
	}

	logger->info("Starting paths");
	for (size_t i = 0; i < list_length(&paths); i++) {
		auto *p = (struct path *) list_at(&paths, i);

		if (p->enabled) {
			ret = path_init2(p);
			if (ret)
				throw new RuntimeError("Failed to prepare path: {}", path_name(p));

			ret = path_start(p);
			if (ret)
				throw new RuntimeError("Failed to start path: {}", path_name(p));
		}
		else
			logger->warn("Path {} is disabled. Skipping...", path_name(p));
	}

#ifdef WITH_HOOKS
	if (stats > 0) {
		stats_print_header(STATS_FORMAT_HUMAN);

		ret = task_init(&task, 1.0 / stats, CLOCK_REALTIME);
		if (ret)
			throw new RuntimeError("Failed to create stats timer");
	}
#endif /* WITH_HOOKS */

	state = STATE_STARTED;

	return 0;
}

int SuperNode::stop()
{
	int ret;

#ifdef WITH_HOOKS
	if (stats > 0) {
		stats_print_footer(STATS_FORMAT_HUMAN);

		ret = task_destroy(&task);
		if (ret)
			throw new RuntimeError("Failed to stop stats timer");
	}
#endif /* WITH_HOOKS */

	logger->info("Stopping paths");
	for (size_t i = 0; i < list_length(&paths); i++) {
		auto *p = (struct path *) list_at(&paths, i);

		ret = path_stop(p);
		if (ret)
			throw new RuntimeError("Failed to stop path: {}", path_name(p));
	}

	logger->info("Stopping nodes");
	for (size_t i = 0; i < list_length(&nodes); i++) {
		auto *n = (struct node *) list_at(&nodes, i);

		ret = node_stop(n);
		if (ret)
			throw new RuntimeError("Failed to stop node: {}", node_name(n));
	}

	logger->info("Stopping node-types");
	for (size_t i = 0; i < list_length(&plugins); i++) {
		auto *p = (struct plugin *) list_at(&plugins, i);

		if (p->type == PLUGIN_TYPE_NODE) {
			ret = node_type_stop(&p->node);
			if (ret)
				throw new RuntimeError("Failed to stop node-type: {}", node_type_name(&p->node));
		}
	}

#ifdef WITH_API
	api.stop();
#endif
#ifdef WITH_WEB
	web.stop();
#endif

	state = STATE_STOPPED;

	return 0;
}

void SuperNode::run()
{
#ifdef WITH_HOOKS
	task_wait(&task);
	periodic();
#else
	pause();
#endif /* WITH_HOOKS */
}

SuperNode::~SuperNode()
{
	assert(state != STATE_STARTED);

	list_destroy(&plugins, (dtor_cb_t) plugin_destroy, false);
	list_destroy(&paths,   (dtor_cb_t) path_destroy, true);
	list_destroy(&nodes,   (dtor_cb_t) node_destroy, true);

	if (json)
		json_decref(json);
}

int SuperNode::periodic()
{
#ifdef WITH_HOOKS
	int ret;

	for (size_t i = 0; i < list_length(&paths); i++) {
		auto *p = (struct path *) list_at(&paths, i);

		if (p->state != STATE_STARTED)
			continue;

		for (size_t j = 0; j < list_length(&p->hooks); j++) {
			hook *h = (struct hook *) list_at(&p->hooks, j);

			ret = hook_periodic(h);
			if (ret)
				return ret;
		}
	}

	for (size_t i = 0; i < list_length(&nodes); i++) {
		auto *n = (struct node *) list_at(&nodes, i);

		if (n->state != STATE_STARTED)
			continue;

		for (size_t j = 0; j < list_length(&n->in.hooks); j++) {
			auto *h = (struct hook *) list_at(&n->in.hooks, j);

			ret = hook_periodic(h);
			if (ret)
				return ret;
		}

		for (size_t j = 0; j < list_length(&n->out.hooks); j++) {
			auto *h = (struct hook *) list_at(&n->out.hooks, j);

			ret = hook_periodic(h);
			if (ret)
				return ret;
		}
	}
#endif
	return 0;
}
