/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include "super_node.h"
#include "node.h"
#include "path.h"
#include "utils.h"
#include "list.h"
#include "hook.h"
#include "advio.h"
#include "web.h"
#include "log.h"
#include "api.h"
#include "plugin.h"
#include "memory.h"
#include "config.h"
#include "json.h"

#include "kernel/rt.h"

static void config_dtor(void *data)
{
	if (data)
		free(data);
}

static int super_node_parse_global(struct super_node *sn, config_setting_t *cfg)
{
	if (!config_setting_is_group(cfg))
		cerror(cfg, "Global section must be a dictionary.");

	config_setting_lookup_int(cfg, "hugepages", &sn->hugepages);
	config_setting_lookup_int(cfg, "affinity", &sn->affinity);
	config_setting_lookup_int(cfg, "priority", &sn->priority);
	config_setting_lookup_float(cfg, "stats", &sn->stats);

	return 0;
}

int super_node_init(struct super_node *sn)
{
	assert(sn->state == STATE_DESTROYED);

	config_init(&sn->cfg);

	log_init(&sn->log, V, LOG_ALL);
	api_init(&sn->api, sn);
	web_init(&sn->web, &sn->api);

	list_init(&sn->nodes);
	list_init(&sn->paths);
	list_init(&sn->plugins);
	
	/* Default values */
	sn->affinity = 0;
	sn->priority = 0;
	sn->stats = 0;
	sn->hugepages = DEFAULT_NR_HUGEPAGES;
	
	sn->state = STATE_INITIALIZED;

	return 0;
}

int super_node_parse_uri(struct super_node *sn, const char *uri)
{
	int ret = CONFIG_FALSE;

	if (uri) { INDENT
		FILE *f;
		AFILE *af;
		config_setting_t *cfg_root = NULL;

		/* Via stdin */
		if (!strcmp("-", uri)) {
			af = NULL;
			f = stdin;

			info("Reading configuration from stdin");
		}
		/* Local file? */
		else if (access(uri, F_OK) != -1) {
			/* Setup libconfig include path.
			 * This is only supported for local files */
			char *uri_cpy = strdup(uri);
			char *include_dir = dirname(uri_cpy);

			config_set_include_dir(&sn->cfg, include_dir);
			
			free(uri_cpy);

			af = NULL;
			f = fopen(uri, "r");
			
			info("Reading configuration from local file: %s", uri);
		}
		/* Use advio (libcurl) to fetch the config from a remote */
		else {
			af = afopen(uri, "r");
			f = af ? af->file : NULL;
			
			info("Reading configuration from URI: %s", uri);
		}

		/* Check if file could be loaded / opened */
		if (!f)
			error("Failed to open configuration");

		config_set_destructor(&sn->cfg, config_dtor);
		config_set_auto_convert(&sn->cfg, 1);

		/* Parse config */
		ret = config_read(&sn->cfg, f);
		if (ret != CONFIG_TRUE) {
			/* This does not seem to be a valid libconfig configuration.
			 * Lets try to parse it as JSON instead. */
			
			json_error_t err;
			json_t *json;
			
			json = json_loadf(f, 0, &err);
			if (json) {
				ret = json_to_config(json, cfg_root);
				if (ret)
					error("Failed t convert JSON to configuration file");
			}
			else {
				info("conf: %s in %s:%d", config_error_text(&sn->cfg), uri, config_error_line(&sn->cfg));
				info("json: %s in %s:%d:%d", err.text, err.source, err.line, err.column);
				error("Failed to parse configuration");
			}
		}

		/* Little hack to properly report configuration filename in error messages
		 * We add the uri as a "hook" object to the root setting.
		 * See cerror() on how this info is used.
		 */
		cfg_root = config_root_setting(&sn->cfg);
		config_setting_set_hook(cfg_root, strdup(uri));

		/* Close configuration file */
		if (af)
			afclose(af);
		else if (f != stdin)
			fclose(f);

		return super_node_parse(sn, cfg_root);
	}
	else { INDENT
		warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");
	}
	
	return 0;
}

int super_node_parse_cli(struct super_node *sn, int argc, char *argv[])
{
	char *uri = (argc == 2) ? argv[1] : NULL;
	
	sn->cli.argc = argc;
	sn->cli.argv = argv;
	
	return super_node_parse_uri(sn, uri);
}

int super_node_parse(struct super_node *sn, config_setting_t *cfg)
{
	int ret;

	assert(sn->state != STATE_STARTED);
	assert(sn->state != STATE_DESTROYED);
	
	config_setting_t *cfg_nodes, *cfg_paths, *cfg_plugins, *cfg_logging, *cfg_web;

	super_node_parse_global(sn, cfg);

	cfg_web = config_setting_get_member(cfg, "http");
	if (cfg_web)
		web_parse(&sn->web, cfg_web);

	/* Parse logging settings */
	cfg_logging = config_setting_get_member(cfg, "logging");
	if (cfg_logging)
		log_parse(&sn->log, cfg_logging);

	/* Parse plugins */
	cfg_plugins = config_setting_get_member(cfg, "plugins");
	if (cfg_plugins) {
		if (!config_setting_is_array(cfg_plugins))
			cerror(cfg_plugins, "Setting 'plugins' must be a list of strings");

		for (int i = 0; i < config_setting_length(cfg_plugins); i++) {
			struct config_setting_t *cfg_plugin = config_setting_get_elem(cfg_plugins, i);
		
			struct plugin plugin = { .state = STATE_DESTROYED };
			
			ret = plugin_init(&plugin);
			if (ret)
				cerror(cfg_plugin, "Failed to initialize plugin");
		
			ret = plugin_parse(&plugin, cfg_plugin);
			if (ret)
				cerror(cfg_plugin, "Failed to parse plugin");
		
			list_push(&sn->plugins, memdup(&plugin, sizeof(plugin)));
		}
	}

	/* Parse nodes */
	cfg_nodes = config_setting_get_member(cfg, "nodes");
	if (cfg_nodes) {
		if (!config_setting_is_group(cfg_nodes))
			warn("Setting 'nodes' must be a group with node name => group mappings.");

		for (int i = 0; i < config_setting_length(cfg_nodes); i++) {
			config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);

			struct plugin *p;
			const char *type;

			/* Required settings */
			if (!config_setting_lookup_string(cfg_node, "type", &type))
				cerror(cfg_node, "Missing node type");

			p = plugin_lookup(PLUGIN_TYPE_NODE, type);
			if (!p)
				cerror(cfg_node, "Invalid node type: %s", type);

			struct node *n = alloc(sizeof(struct node));
			
			n->state = STATE_DESTROYED;
			
			ret = node_init(n, &p->node);
			if (ret)
				cerror(cfg_node, "Failed to initialize node");

			ret = node_parse(n, cfg_node);
			if (ret)
				cerror(cfg_node, "Failed to parse node");
		
			list_push(&sn->nodes, n);
		}
	}

	/* Parse paths */
	cfg_paths = config_setting_get_member(cfg, "paths");
	if (cfg_paths) {
		if (!config_setting_is_list(cfg_paths))
			warn("Setting 'paths' must be a list.");

		for (int i = 0; i < config_setting_length(cfg_paths); i++) {
			config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);

			struct path p = { .state = STATE_DESTROYED };
			
			ret = path_init(&p, sn);
			if (ret)
				cerror(cfg_path, "Failed to init path");

			ret = path_parse(&p, cfg_path, &sn->nodes);
			if (ret)
				cerror(cfg_path, "Failed to parse path");
		
			list_push(&sn->paths, memdup(&p, sizeof(p)));

			if (p.reverse) {
				struct path r = { .state = STATE_DESTROYED };
				
				ret = path_init(&r, sn);
				if (ret)
					cerror(cfg_path, "Failed to init path");
	
				ret = path_reverse(&p, &r);
				if (ret)
					cerror(cfg_path, "Failed to reverse path %s", path_name(&p));

				list_push(&sn->paths, memdup(&r, sizeof(p)));
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
		struct node *n = list_at(&sn->nodes, i);

		ret = node_check(n);
		if (ret)
			error("Invalid configuration for node %s", node_name(n));
	}
	
	for (size_t i = 0; i < list_length(&sn->paths); i++) {
		struct path *p = list_at(&sn->paths, i);

		ret = path_check(p);
		if (ret)
			error("Invalid configuration for path %s", path_name(p));
	}
	
	sn->state = STATE_CHECKED;

	return 0;
}

int super_node_start(struct super_node *sn)
{
	assert(sn->state == STATE_CHECKED);

	memory_init(sn->hugepages);
	rt_init(sn->priority, sn->affinity);

	log_start(&sn->log);
	api_start(&sn->api);
	web_start(&sn->web);
	
	info("Start node types");
	for (size_t i = 0; i < list_length(&sn->nodes); i++) { INDENT
		struct node *n = list_at(&sn->nodes, i);
		
		node_type_start(n->_vt, sn);
	}
	
	info("Starting nodes");
	for (size_t i = 0; i < list_length(&sn->nodes); i++) { INDENT
		struct node *n = list_at(&sn->nodes, i);

		int refs = list_count(&sn->paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0)
			node_start(n);
		else
			warn("No path is using the node %s. Skipping...", node_name(n));
	}

	info("Starting paths");
	for (size_t i = 0; i < list_length(&sn->paths); i++) { INDENT
		struct path *p = list_at(&sn->paths, i);
		
		if (p->enabled) {
			path_init2(p);
			path_start(p);
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
		struct path *p = list_at(&sn->paths, i);

		ret = path_stop(p);
		if (ret)
			error("Failed to stop path: %s", path_name(p));
	}

	info("Stopping nodes");
	for (size_t i = 0; i < list_length(&sn->nodes); i++) { INDENT
		struct node *n = list_at(&sn->nodes, i);

		ret = node_stop(n);
		if (ret)
			error("Failed to stop node: %s", node_name(n));
	}

	info("Stopping node types");
	for (size_t i = 0; i < list_length(&plugins); i++) { INDENT
		struct plugin *p = list_at(&plugins, i);

		if (p->type == PLUGIN_TYPE_NODE) {
			ret = node_type_stop(&p->node);
			if (ret)
				error("Failed to stop node-type: %s", plugin_name(p));
		}
	}

	web_stop(&sn->web);
	api_stop(&sn->api);
	log_stop(&sn->log);
	
	sn->state = STATE_STOPPED;
	
	return 0;
}

int super_node_destroy(struct super_node *sn)
{
	assert(sn->state != STATE_DESTROYED);

	list_destroy(&sn->plugins, (dtor_cb_t) plugin_destroy, false);
	list_destroy(&sn->paths,   (dtor_cb_t) path_destroy, true);
	list_destroy(&sn->nodes,   (dtor_cb_t) node_destroy, true);

	web_destroy(&sn->web);
	api_destroy(&sn->api);
	log_destroy(&sn->log);

	config_destroy(&sn->cfg);

	sn->state = STATE_DESTROYED;
	
	return 0;
}
