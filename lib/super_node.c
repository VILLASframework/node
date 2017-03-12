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
	
	sn->web.port = 80;
	sn->web.htdocs = "/villas/web/socket/";
	
	sn->state = STATE_INITIALIZED;

	return 0;
}

int super_node_parse_uri(struct super_node *sn, const char *uri)
{
	info("Parsing configuration: uri=%s", uri);
	
	int ret = CONFIG_FALSE;

	if (uri) { INDENT
		FILE *f;
		AFILE *af;
		config_setting_t *cfg_root;

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

			config_set_include_dir(&sn->cfg, include_dir);
			
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
		config_set_auto_convert(&sn->cfg, 1);
		ret = config_read(&sn->cfg, f);
		if (ret != CONFIG_TRUE)
			error("Failed to parse configuration: %s in %s:%d", config_error_text(&sn->cfg), uri, config_error_line(&sn->cfg));

		cfg_root = config_root_setting(&sn->cfg);

		/* Little hack to properly report configuration filename in error messages
		 * We add the uri as a "hook" object to the root setting.
		 * See cerror() on how this info is used.
		 */
		config_setting_set_hook(cfg_root, strdup(uri));
		config_set_destructor(&sn->cfg, config_dtor);

		/* Close configuration file */
		if (af)
			afclose(af);
		else
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
		
			struct plugin plugin;
		
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
		
			struct node n = { .state = STATE_DESTROYED };
			
			ret = node_init(&n, &p->node);
			if (ret)
				cerror(cfg_node, "Failed to initialize node");

			ret = node_parse(&n, cfg_node);
			if (ret)
				cerror(cfg_node, "Failed to parse node");
		
			list_push(&sn->nodes, memdup(&n, sizeof(n)));
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
				struct path r;
				
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

	list_foreach(struct node *n, &sn->nodes) {
		ret = node_check(n);
		if (ret)
			error("Invalid configuration for node %s", node_name(n));
	}
	
	list_foreach(struct path *p, &sn->paths) {
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

	api_start(&sn->api);
	web_start(&sn->web);
	
	info("Start node types");
	list_foreach(struct node *n, &sn->nodes) { INDENT
		config_setting_t *cfg = config_root_setting(&sn->cfg);
		
		node_type_start(n->_vt, sn->cli.argc, sn->cli.argv, cfg);
	}
	
	info("Starting nodes");
	list_foreach(struct node *n, &sn->nodes) { INDENT
		int refs = list_count(&sn->paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0)
			node_start(n);
		else
			warn("No path is using the node %s. Skipping...", node_name(n));
	}

	info("Starting paths");
	list_foreach(struct path *p, &sn->paths) { INDENT
		if (p->enabled) {
			path_init(p, sn);
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
	assert(sn->state == STATE_STARTED);

	info("Stopping paths");
	list_foreach(struct path *p, &sn->paths) { INDENT
		path_stop(p);
	}

	info("Stopping nodes");
	list_foreach(struct node *n, &sn->nodes) { INDENT
		node_stop(n);
	}

	info("De-initializing node types");
	list_foreach(struct plugin *p, &plugins) { INDENT
		if (p->type == PLUGIN_TYPE_NODE)
			node_type_stop(&p->node);
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

	config_destroy(&sn->cfg);

	web_destroy(&sn->web);
	log_destroy(&sn->log);
	api_destroy(&sn->api);

	list_destroy(&sn->plugins, (dtor_cb_t) plugin_destroy, false);
	list_destroy(&sn->paths,   (dtor_cb_t) path_destroy, true);
	list_destroy(&sn->nodes,   (dtor_cb_t) node_destroy, true);
	
	sn->state = STATE_DESTROYED;
	
	return 0;
}