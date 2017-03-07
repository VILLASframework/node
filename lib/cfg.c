/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include "utils.h"
#include "list.h"
#include "cfg.h"
#include "node.h"
#include "path.h"
#include "hook.h"
#include "advio.h"
#include "web.h"
#include "log.h"
#include "api.h"
#include "plugin.h"
#include "node.h"

#include "kernel/rt.h"

int cfg_init_pre(struct cfg *cfg)
{
	config_init(&cfg->cfg);
	
	info("Inititliaze logging sub-system");
	log_init(&cfg->log, V, LOG_ALL);

	list_init(&cfg->nodes);
	list_init(&cfg->paths);
	list_init(&cfg->plugins);

	return 0;
}

int cfg_init_post(struct cfg *cfg)
{
	memory_init();
	rt_init(cfg->priority, cfg->affinity);
	api_init(&cfg->api, cfg);
	web_init(&cfg->web, &cfg->api);
	
	info("Initialize node types");
	list_foreach(struct node *n, &cfg->nodes) { INDENT
		config_setting_t *cfg_root = config_root_setting(&cfg->cfg);
		
		node_type_init(n->_vt, cfg->cli.argc, cfg->cli.argv, cfg_root);
	}
	
	return 0;
}

int cfg_deinit(struct cfg *cfg)
{
	info("De-initializing node types");
	list_foreach(struct plugin *p, &plugins) { INDENT
		if (p->type == PLUGIN_TYPE_NODE)
			node_type_deinit(&p->node);
	}
	
	info("De-initializing web interface");
	web_deinit(&cfg->web);
	
	info("De-initialize API");
	api_deinit(&cfg->api);
	
	info("De-initialize log sub-system");
	log_deinit(&cfg->log);
	
	return 0;
}

int cfg_destroy(struct cfg *cfg)
{
	config_destroy(&cfg->cfg);

	web_destroy(&cfg->web);
	log_destroy(&cfg->log);
	api_destroy(&cfg->api);

	list_destroy(&cfg->plugins, (dtor_cb_t) plugin_destroy, false);
	list_destroy(&cfg->paths,   (dtor_cb_t) path_destroy, true);
	list_destroy(&cfg->nodes,   (dtor_cb_t) node_destroy, true);
	
	return 0;
}

int cfg_parse_cli(struct cfg *cfg, int argc, char *argv[])
{
	cfg->cli.argc = argc;
	cfg->cli.argv = argv;
	
	char *uri = (argc == 2) ? argv[1] : NULL;
	
	return cfg_parse(cfg, uri);
}

int cfg_parse(struct cfg *cfg, const char *uri)
{
	config_setting_t *cfg_root, *cfg_nodes, *cfg_paths, *cfg_plugins, *cfg_logging, *cfg_web;

	info("Parsing configuration: uri=%s", uri);
	
	{ INDENT

		int ret = CONFIG_FALSE;

		if (uri) {
			/* Setup libconfig */
			config_set_auto_convert(&cfg->cfg, 1);

			FILE *f;
			AFILE *af;
	
			/* Via stdin */
			if (strcmp("-", uri) == 0) {
				af = NULL;
				f = stdin;
			}
			/* Local file? */
			else if (access(uri, F_OK) != -1) {
				/* Setup libconfig include path.
				 * This is only supported for local files */
				char *uri_cpy = strdup(uri);
				char *include_dir = dirname(uri_cpy);

				config_set_include_dir(&cfg->cfg, include_dir);
				
				free(uri_cpy);

				af = NULL;
				f = fopen(uri, "r");
			}
			/* Use advio (libcurl) to fetch the config from a remote */
			else {
				af = afopen(uri, "r", ADVIO_MEM);
				f = af ? af->file : NULL;
			}

			/* Check if file could be loaded / opened */
			if (!f)
				error("Failed to open configuration from: %s", uri);
		
			/* Parse config */
			ret = config_read(&cfg->cfg, f);
			if (ret != CONFIG_TRUE)
				error("Failed to parse configuration: %s in %s:%d", config_error_text(&cfg->cfg), uri, config_error_line(&cfg->cfg));

			/* Close configuration file */
			if (af)
				afclose(af);
			else
				fclose(f);
		}
		else {
			warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");

			cfg->web.port = 80;
			cfg->web.htdocs = "/villas/web/socket/";
		}

		/* Parse global settings */
		cfg_root = config_root_setting(&cfg->cfg);
		if (cfg_root) {
			if (!config_setting_is_group(cfg_root))
				warn("Missing global section in config file.");

			if (!config_setting_lookup_int(cfg_root, "affinity", &cfg->affinity))
				cfg->affinity = 0;

			if (!config_setting_lookup_int(cfg_root, "priority", &cfg->priority))
				cfg->priority = 0;

			if (!config_setting_lookup_float(cfg_root, "stats", &cfg->stats))
				cfg->stats = 0;
		}
	
		cfg_web = config_setting_get_member(cfg_root, "http");
		if (cfg_web)
			web_parse(&cfg->web, cfg_web);

		/* Parse logging settings */
		cfg_logging = config_setting_get_member(cfg_root, "logging");
		if (cfg_logging)
			log_parse(&cfg->log, cfg_logging);

		/* Parse plugins */
		cfg_plugins = config_setting_get_member(cfg_root, "plugins");
		if (cfg_plugins) {
			if (!config_setting_is_array(cfg_plugins))
				cerror(cfg_plugins, "Setting 'plugins' must be a list of strings");

			for (int i = 0; i < config_setting_length(cfg_plugins); i++) {
				struct config_setting_t *cfg_plugin = config_setting_get_elem(cfg_plugins, i);
			
				struct plugin plugin;
			
				ret = plugin_parse(&plugin, cfg_plugin);
				if (ret)
					cerror(cfg_plugin, "Failed to parse plugin");
			
				list_push(&cfg->plugins, memdup(&plugin, sizeof(plugin)));
			}
		}

		/* Parse nodes */
		cfg_nodes = config_setting_get_member(cfg_root, "nodes");
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
			
				struct node *n = node_create(&p->node);

				ret = node_parse(n, cfg_node);
				if (ret)
					cerror(cfg_node, "Failed to parse node");
			
				list_push(&cfg->nodes, n);
			}
		}

		/* Parse paths */
		cfg_paths = config_setting_get_member(cfg_root, "paths");
		if (cfg_paths) {
			if (!config_setting_is_list(cfg_paths))
				warn("Setting 'paths' must be a list.");

			for (int i = 0; i < config_setting_length(cfg_paths); i++) {
				config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);

				struct path *p = path_create();
			
				ret = path_parse(p, cfg_path, &cfg->nodes);
				if (ret)
					cerror(cfg_path, "Failed to parse path");
			
				list_push(&cfg->paths, p);

				if (p->reverse) {
					struct path *r = path_create();
		
					ret = path_reverse(p, r);
					if (ret)
						cerror(cfg_path, "Failed to reverse path %s", path_name(p));

					list_push(&cfg->paths, r);
				}
			}
		}
	}

	return 0;
}