/** The super node object holding the state of the application.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/config.h>
#include <villas/super_node.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/utils.h>
#include <villas/list.h>
#include <villas/hook.h>
#include <villas/advio.h>
#include <villas/web.h>
#include <villas/log.h>
#include <villas/api.h>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/config.h>
#include <villas/config_helper.h>

#include <villas/kernel/rt.h>

int super_node_init(struct super_node *sn)
{
	assert(sn->state == STATE_DESTROYED);

	log_init(&sn->log, V, LOG_ALL);
#ifdef WITH_API
	api_init(&sn->api, sn);
#endif /* WITH_API */
#ifdef WITH_WEB
	web_init(&sn->web, &sn->api);
#endif /* WITH_WEB */

	list_init(&sn->nodes);
	list_init(&sn->paths);
	list_init(&sn->plugins);

	/* Default values */
	sn->affinity = 0;
	sn->priority = 0;
	sn->stats = 0;
	sn->hugepages = DEFAULT_NR_HUGEPAGES;

	sn->name = alloc(128); /** @todo missing free */
	gethostname(sn->name, 128);

	sn->state = STATE_INITIALIZED;

	return 0;
}

int super_node_parse_uri(struct super_node *sn, const char *uri)
{
	json_error_t err;

	info("Parsing configuration");

	if (uri) { INDENT
		FILE *f;
		AFILE *af;

		/* Via stdin */
		if (!strcmp("-", uri)) {
			info("Reading configuration from stdin");

			af = NULL;
			f = stdin;
		}
		else {
			info("Reading configuration from URI: %s", uri);

			af = afopen(uri, "r");
			if (!af)
				error("Failed to open configuration from: %s", uri);

			f = af->file;
		}

		/* Parse config */
		sn->cfg = json_loadf(f, 0, &err);
		if (sn->cfg == NULL) {
#ifdef WITH_CONFIG
			int ret;

			config_t cfg;
			config_setting_t *json_root = NULL;

			warn("Failed to parse JSON configuration. Re-trying with old libconfig format.");
			{ INDENT
				warn("Please consider migrating to the new format using the 'conf2json' command.");
			}

			config_init(&cfg);
			config_set_auto_convert(&cfg, 1);

			/* Setup libconfig include path.
			 * This is only supported for local files */
			if (access(uri, F_OK) != -1) {
				char *cpy = strdup(uri);

				config_set_include_dir(&cfg, dirname(cpy));

				free(cpy);
			}

			if (af)
				arewind(af);
			else
				rewind(f);

			ret = config_read(&cfg, f);
			if (ret != CONFIG_TRUE) {
				{ INDENT
					warn("conf: %s in %s:%d", config_error_text(&cfg), uri, config_error_line(&cfg));
					warn("json: %s in %s:%d:%d", err.text, err.source, err.line, err.column);
				}
				error("Failed to parse configuration");
			}

			json_root = config_root_setting(&cfg);

			sn->cfg = config_to_json(json_root);
			if (sn->cfg == NULL)
				error("Failed to convert JSON to configuration file");

			config_destroy(&cfg);
#else
			jerror(&err, "Failed to parse configuration file");
#endif /* WITH_CONFIG */
		}

		/* Close configuration file */
		if (af)
			afclose(af);
		else if (f != stdin)
			fclose(f);

		sn->uri = strdup(uri);

		return super_node_parse_json(sn, sn->cfg);
	}
	else { INDENT
		warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");
	}

	return 0;
}

int super_node_parse_json(struct super_node *sn, json_t *cfg)
{
	int ret;
	const char *name = NULL;

	assert(sn->state != STATE_STARTED);
	assert(sn->state != STATE_DESTROYED);

	json_t *json_nodes = NULL;
	json_t *json_paths = NULL;
	json_t *json_plugins = NULL;
	json_t *json_logging = NULL;
	json_t *json_web = NULL;

	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o, s?: o, s?: o, s?: o, s?: o, s?: i, s?: i, s?: i, s?: F, s?: s }",
		"http", &json_web,
		"logging", &json_logging,
		"plugins", &json_plugins,
		"nodes", &json_nodes,
		"paths", &json_paths,
		"hugepages", &sn->hugepages,
		"affinity", &sn->affinity,
		"priority", &sn->priority,
		"stats", &sn->stats,
		"name", &name
	);
	if (ret)
		jerror(&err, "Failed to parse global configuration");

	if (name) {
		sn->name = realloc(sn->name, strlen(name)+1);
		sprintf(sn->name, "%s", name);
	}

#ifdef WITH_WEB
	if (json_web)
		web_parse(&sn->web, json_web);
#endif /* WITH_WEB */

	if (json_logging)
		log_parse(&sn->log, json_logging);

	/* Parse plugins */
	if (json_plugins) {
		if (!json_is_array(json_plugins))
			error("Setting 'plugins' must be a list of strings");

		size_t index;
		json_t *json_plugin;
		json_array_foreach(json_plugins, index, json_plugin) {
			struct plugin *p = (struct plugin *) alloc(sizeof(struct plugin));

			ret = plugin_init(p);
			if (ret)
				error("Failed to initialize plugin");

			ret = plugin_parse(p, json_plugin);
			if (ret)
				error("Failed to parse plugin");

			list_push(&sn->plugins, p);
		}
	}

	/* Parse nodes */
	if (json_nodes) {
		if (!json_is_object(json_nodes))
			error("Setting 'nodes' must be a group with node name => group mappings.");

		const char *name;
		json_t *json_node;
		json_object_foreach(json_nodes, name, json_node) {
			struct node_type *nt;
			const char *type;

			ret = json_unpack_ex(json_node, &err, 0, "{ s: s }", "type", &type);
			if (ret)
				jerror(&err, "Failed to parse node");

			nt = node_type_lookup(type);
			if (!nt)
				error("Invalid node type: %s", type);

			struct node *n = (struct node *) alloc(sizeof(struct node));

			ret = node_init(n, nt);
			if (ret)
				error("Failed to initialize node");

			ret = node_parse(n, json_node, name);
			if (ret)
				error("Failed to parse node");

			list_push(&sn->nodes, n);
		}
	}

	/* Parse paths */
	if (json_paths) {
		if (!json_is_array(json_paths))
			warn("Setting 'paths' must be a list.");

		size_t index;
		json_t *json_path;
		json_array_foreach(json_paths, index, json_path) {
			struct path *p = (struct path *) alloc(sizeof(struct path));

			ret = path_init(p);
			if (ret)
				error("Failed to initialize path");

			ret = path_parse(p, json_path, &sn->nodes);
			if (ret)
				error("Failed to parse path");

			list_push(&sn->paths, p);

			if (p->reverse) {
				struct path *r = (struct path *) alloc(sizeof(struct path));

				ret = path_init(r);
				if (ret)
					error("Failed to init path");

				ret = path_reverse(p, r);
				if (ret)
					error("Failed to reverse path %s", path_name(p));

				list_push(&sn->paths, r);
			}
		}
	}

	sn->state = STATE_PARSED;

	return 0;
}

int super_node_check(struct super_node *sn)
{
	int ret;

	assert(sn->state != STATE_DESTROYED);

	for (size_t i = 0; i < list_length(&sn->nodes); i++) {
		struct node *n = (struct node *) list_at(&sn->nodes, i);

		ret = node_check(n);
		if (ret)
			error("Invalid configuration for node %s", node_name(n));
	}

	for (size_t i = 0; i < list_length(&sn->paths); i++) {
		struct path *p = (struct path *) list_at(&sn->paths, i);

		ret = path_check(p);
		if (ret)
			error("Invalid configuration for path %s", path_name(p));
	}

	sn->state = STATE_CHECKED;

	return 0;
}

int super_node_start(struct super_node *sn)
{
	int ret;

	assert(sn->state == STATE_CHECKED);

	memory_init(sn->hugepages);
	rt_init(sn->priority, sn->affinity);

	log_open(&sn->log);
#ifdef WITH_API
	api_start(&sn->api);
#endif
#ifdef WITH_WEB
	web_start(&sn->web);
#endif

	info("Starting node-types");
	for (size_t i = 0; i < list_length(&sn->nodes); i++) { INDENT
		struct node *n = (struct node *) list_at(&sn->nodes, i);

		ret = node_type_start(n->_vt, sn);
		if (ret)
			error("Failed to start node-type: %s", plugin_name(n->_vt));
	}

	info("Starting nodes");
	for (size_t i = 0; i < list_length(&sn->nodes); i++) { INDENT
		struct node *n = (struct node *) list_at(&sn->nodes, i);

		int refs = list_count(&sn->paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0) { INDENT
			ret = node_start(n);
			if (ret)
				error("Failed to start node: %s", node_name(n));
		}
		else
			warn("No path is using the node %s. Skipping...", node_name(n));
	}

	info("Starting paths");
	for (size_t i = 0; i < list_length(&sn->paths); i++) { INDENT
		struct path *p = (struct path *) list_at(&sn->paths, i);

		if (p->enabled) { INDENT
			ret = path_init2(p);
			if (ret)
				error("Failed to start path: %s", path_name(p));

			ret = path_start(p);
			if (ret)
				error("Failed to start path: %s", path_name(p));
		}
		else
			warn("Path %s is disabled. Skipping...", path_name(p));
	}

	sn->state = STATE_STARTED;

	return 0;
}

int super_node_stop(struct super_node *sn)
{
	int ret;

	info("Stopping paths");
	for (size_t i = 0; i < list_length(&sn->paths); i++) { INDENT
		struct path *p = (struct path *) list_at(&sn->paths, i);

		ret = path_stop(p);
		if (ret)
			error("Failed to stop path: %s", path_name(p));
	}

	info("Stopping nodes");
	for (size_t i = 0; i < list_length(&sn->nodes); i++) { INDENT
		struct node *n = (struct node *) list_at(&sn->nodes, i);

		ret = node_stop(n);
		if (ret)
			error("Failed to stop node: %s", node_name(n));
	}

	info("Stopping node-types");
	for (size_t i = 0; i < list_length(&plugins); i++) { INDENT
		struct plugin *p = (struct plugin *) list_at(&plugins, i);

		if (p->type == PLUGIN_TYPE_NODE) {
			ret = node_type_stop(&p->node);
			if (ret)
				error("Failed to stop node-type: %s", plugin_name(p));
		}
	}

#ifdef WITH_API
	api_stop(&sn->api);
#endif
#ifdef WITH_WEB
	web_stop(&sn->web);
#endif
	log_close(&sn->log);

	sn->state = STATE_STOPPED;

	return 0;
}

int super_node_destroy(struct super_node *sn)
{
	assert(sn->state != STATE_DESTROYED);

	list_destroy(&sn->plugins, (dtor_cb_t) plugin_destroy, false);
	list_destroy(&sn->paths,   (dtor_cb_t) path_destroy, true);
	list_destroy(&sn->nodes,   (dtor_cb_t) node_destroy, true);

#ifdef WITH_WEB
	web_destroy(&sn->web);
#endif /* WITH_WEB */
#ifdef WITH_API
	api_destroy(&sn->api);
#endif /* WITH_API */

	json_decref(sn->cfg);
	log_destroy(&sn->log);

	if (sn->name)
		free(sn->name);

	sn->state = STATE_DESTROYED;

	return 0;
}
