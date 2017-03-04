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
#include "hooks.h"
#include "advio.h"
#include "web.h"
#include "log.h"
#include "api.h"
#include "plugin.h"

#include "kernel/rt.h"

int cfg_init_pre(struct cfg *cfg)
{
	config_init(cfg->cfg);
	
	info("Inititliaze logging sub-system");
	log_init(&cfg->log);

	list_init(&cfg->nodes);
	list_init(&cfg->paths);
	list_init(&cfg->plugins);

	return 0;
}

int cfg_init_post(struct cfg *cfg)
{
	info("Initialize real-time sub-system");
	rt_init(cfg);

	info("Initialize hook sub-system");
	hook_init(cfg);
	
	info("Initialize API sub-system");
	api_init(&cfg->api, cfg);
	
	info("Initialize web sub-system");
	web_init(&cfg->web, &cfg->api);
	
	return 0;
}

int cfg_deinit(struct cfg *cfg)
{
	info("De-initializing node types");
	list_foreach(struct node_type *vt, &node_types) { INDENT
		node_deinit(vt);
	}
	
	info("De-initializing web interface");
	web_deinit(&cfg->web);
	
	info("De-initialize API");
	api_deinit(&cfg->api);
	
	return 0;
}

int cfg_destroy(struct cfg *cfg)
{
	config_destroy(cfg->cfg);

	web_destroy(&cfg->web);
	log_destroy(&cfg->log);
	api_destroy(&cfg->api);

	list_destroy(&cfg->plugins, (dtor_cb_t) plugin_destroy, false);
	list_destroy(&cfg->paths,   (dtor_cb_t) path_destroy, true);
	list_destroy(&cfg->nodes,   (dtor_cb_t) node_destroy, true);
	
	return 0;
}

int cfg_parse(struct cfg *cfg, const char *uri)
{
	config_setting_t *cfg_root, *cfg_nodes, *cfg_paths, *cfg_plugins, *cfg_logging;

	int ret = CONFIG_FALSE;

	if (uri) {
		/* Setup libconfig */
		config_set_auto_convert(cfg->cfg, 1);

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

			config_set_include_dir(cfg->cfg, include_dir);
				
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
		ret = config_read(cfg->cfg, f);
		if (ret != CONFIG_TRUE)
			error("Failed to parse configuration: %s in %s:%d", config_error_text(cfg->cfg), uri, config_error_line(cfg->cfg));

		/* Close configuration file */
		if (af)
			afclose(af);
		else
			fclose(f);
	}
	else
		warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");

	/* Parse global settings */
	cfg_root = config_root_setting(cfg->cfg);
	if (cfg_root) {
		if (!config_setting_is_group(cfg_root))
			warn("Missing global section in config file.");

		cfg_parse_global(cfg_root, cfg);
	}

	/* Parse logging settings */
	cfg_logging = config_setting_get_member(cfg_root, "logging");
	if (cfg_logging) {
		if (!config_setting_is_group(cfg_logging))
			cerror(cfg_logging, "Setting 'logging' must be a group.");
		
		log_parse(&cfg->log, cfg_logging);
	}

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
			
			list_push(&cfg->plugins, &plugin);
		}
	}

	/* Parse nodes */
	cfg_nodes = config_setting_get_member(cfg_root, "nodes");
	if (cfg_nodes) {
		if (!config_setting_is_group(cfg_nodes))
			warn("Setting 'nodes' must be a group with node name => group mappings.");

		for (int i = 0; i < config_setting_length(cfg_nodes); i++) {
			config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);
			cfg_parse_node(cfg_node, &cfg->nodes, cfg);
		}
	}

	/* Parse paths */
	cfg_paths = config_setting_get_member(cfg_root, "paths");
	if (cfg_paths) {
		if (!config_setting_is_list(cfg_paths))
			warn("Setting 'paths' must be a list.");

		for (int i = 0; i < config_setting_length(cfg_paths); i++) {
			config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);
			cfg_parse_path(cfg_path, &cfg->paths, &cfg->nodes, cfg);
		}
	}

	return 0;
}

int cfg_parse_global(config_setting_t *cfg, struct cfg *set)
{
	if (!config_setting_lookup_int(cfg, "affinity", &set->affinity))
		set->affinity = 0;

	if (!config_setting_lookup_int(cfg, "priority", &set->priority))
		set->priority = 0;

	if (!config_setting_lookup_float(cfg, "stats", &set->stats))
		set->stats = 0;

	return 0;
}

int cfg_parse_path(config_setting_t *cfg,
	struct list *paths, struct list *nodes, struct cfg *set)
{
	config_setting_t *cfg_out, *cfg_hook;
	const char *in;
	int ret, reverse;
	struct path *p;
	
	/* Allocate memory and intialize path structure */
	p = alloc(sizeof(struct path));
	path_init(p);

	/* Input node */
	if (!config_setting_lookup_string(cfg, "in", &in) &&
	    !config_setting_lookup_string(cfg, "from", &in) &&
	    !config_setting_lookup_string(cfg, "src", &in) &&
	    !config_setting_lookup_string(cfg, "source", &in))
		cerror(cfg, "Missing input node for path");

	p->in = list_lookup(nodes, in);
	if (!p->in)
		cerror(cfg, "Invalid input node '%s'", in);

	/* Output node(s) */
	if (!(cfg_out = config_setting_get_member(cfg, "out")) &&
	    !(cfg_out = config_setting_get_member(cfg, "to")) &&
	    !(cfg_out = config_setting_get_member(cfg, "dst")) &&
	    !(cfg_out = config_setting_get_member(cfg, "dest")) &&
	    !(cfg_out = config_setting_get_member(cfg, "sink")))
		cerror(cfg, "Missing output nodes for path");
	
	ret = cfg_parse_nodelist(cfg_out, &p->destinations, nodes);
	if (ret <= 0)
		cerror(cfg_out, "Invalid output nodes");
	
	/* Check if nodes are suitable */
	if (p->in->_vt->read == NULL)
		cerror(cfg, "Input node '%s' is not supported as a source.", node_name(p->in));
	
	list_foreach(struct node *n, &p->destinations) {
		if (n->_vt->write == NULL)
			cerror(cfg_out, "Output node '%s' is not supported as a destination.", node_name(n));
	}

	/* Optional settings */
	cfg_hook = config_setting_get_member(cfg, "hook");
	if (cfg_hook)
		cfg_parse_hooklist(cfg_hook, &p->hooks);

	if (!config_setting_lookup_int(cfg, "values", &p->samplelen))
		p->samplelen = DEFAULT_VALUES;
	if (!config_setting_lookup_int(cfg, "queuelen", &p->queuelen))
		p->queuelen = DEFAULT_QUEUELEN;
	if (!config_setting_lookup_bool(cfg, "reverse", &reverse))
		reverse = 0;
	if (!config_setting_lookup_bool(cfg, "enabled", &p->enabled))
		p->enabled = 1;
	if (!config_setting_lookup_float(cfg, "rate", &p->rate))
		p->rate = 0; /* disabled */

	p->cfg = cfg;

	list_push(paths, p);

	if (reverse) {
		if (list_length(&p->destinations) > 1)
			cerror(cfg, "Can't reverse path with multiple destination nodes");

		struct path *r = memdup(p, sizeof(struct path));
		path_init(r);

		/* Swap source and destination node */
		r->in = list_first(&p->destinations);
		list_push(&r->destinations, p->in);
			
		if (cfg_hook)
			cfg_parse_hooklist(cfg_hook, &r->hooks);

		list_push(paths, r);
	}

	return 0;
}

int cfg_parse_nodelist(config_setting_t *cfg, struct list *list, struct list *all) {
	const char *str;
	struct node *node;

	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			str = config_setting_get_string(cfg);
			if (str) {
				node = list_lookup(all, str);
				if (node)
					list_push(list, node);
				else
					cerror(cfg, "Unknown outgoing node '%s'", str);
			}
			else
				cerror(cfg, "Invalid outgoing node");
			break;

		case CONFIG_TYPE_ARRAY:
			for (int i = 0; i < config_setting_length(cfg); i++) {
				config_setting_t *elm = config_setting_get_elem(cfg, i);
				
				str = config_setting_get_string(elm);
				if (str) {
					node = list_lookup(all, str);
					if (!node)
						cerror(elm, "Unknown outgoing node '%s'", str);
					else if (node->_vt->write == NULL)
						cerror(cfg, "Output node '%s' is not supported as a sink.", node_name(node));

					list_push(list, node);
				}
				else
					cerror(cfg, "Invalid outgoing node");
			}
			break;

		default:
			cerror(cfg, "Invalid output node(s)");
	}

	return list_length(list);
}

int cfg_parse_node(config_setting_t *cfg, struct list *nodes, struct cfg *set)
{
	const char *type;
	int ret;

	struct node *n;
	struct node_type *vt;

	/* Required settings */
	if (!config_setting_lookup_string(cfg, "type", &type))
		cerror(cfg, "Missing node type");

	vt = list_lookup(&node_types, type);
	if (!vt)
		cerror(cfg, "Invalid type for node '%s'", config_setting_name(cfg));
	
	n = node_create(vt);

	n->name = config_setting_name(cfg);
	n->cfg = cfg;

	ret = node_parse(n, cfg);
	if (ret)
		cerror(cfg, "Failed to parse node '%s'", node_name(n));

	if (config_setting_lookup_int(cfg, "vectorize", &n->vectorize)) {
		config_setting_t *cfg_vectorize = config_setting_lookup(cfg, "vectorize");
		
		if (n->vectorize <= 0)
			cerror(cfg_vectorize, "Invalid value for `vectorize` %d. Must be natural number!", n->vectorize);
		if (vt->vectorize && vt->vectorize < n->vectorize)
			cerror(cfg_vectorize, "Invalid value for `vectorize`. Node type %s requires a number smaller than %d!",
				node_name_type(n), vt->vectorize);
	}
	else
		n->vectorize = 1;

	if (!config_setting_lookup_int(cfg, "affinity", &n->affinity))
		n->affinity = set->affinity;
	
	list_push(nodes, n);

	return ret;
}

int cfg_parse_hooklist(config_setting_t *cfg, struct list *list) {
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			cfg_parse_hook(cfg, list);
			break;

		case CONFIG_TYPE_ARRAY:
			for (int i = 0; i < config_setting_length(cfg); i++)
				cfg_parse_hook(config_setting_get_elem(cfg, i), list);
			break;

		default:
			cerror(cfg, "Invalid hook functions");
	}

	return list_length(list);
}

int cfg_parse_hook(config_setting_t *cfg, struct list *list)
{
	struct hook *hook, *copy;
	char *name, *param;
	const char *hookline = config_setting_get_string(cfg);
	if (!hookline)
		cerror(cfg, "Invalid hook function");
	
	name  = strtok((char *) hookline, ":");
	param = strtok(NULL, "");

	debug(3, "Hook: %s => %s", name, param);

	hook = list_lookup(&hooks, name);
	if (!hook)
		cerror(cfg, "Unknown hook function '%s'", name);
		
	copy = memdup(hook, sizeof(struct hook));
	copy->parameter = param;
	
	list_push(list, copy);

	return 0;
}
