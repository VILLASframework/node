/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <dlfcn.h>
#include <unistd.h>

#include "utils.h"
#include "list.h"
#include "cfg.h"
#include "node.h"
#include "path.h"
#include "hooks.h"

void cfg_init(config_t *cfg)
{
	config_init(cfg);
}

void cfg_destroy(config_t *cfg)
{
	config_destroy(cfg);
}

int cfg_parse(const char *filename, config_t *cfg, struct settings *set,
	struct list *nodes, struct list *paths)
{
	int ret = CONFIG_FALSE;
	char *filename_cpy, *include_dir;

	config_init(cfg);

	filename_cpy = strdup(filename);
	include_dir = dirname(filename_cpy);

	/* Setup libconfig */
	config_set_auto_convert(cfg, 1);
	config_set_include_dir(cfg, include_dir);

	free(filename_cpy);
	
	if (strcmp("-", filename) == 0)
		ret = config_read(cfg, stdin);
	else if (access(filename, F_OK) != -1)
		ret = config_read_file(cfg, filename);
	else
		error("Invalid configuration file name: %s", filename);

	if (ret != CONFIG_TRUE) {
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg),
			config_error_file(cfg) ? config_error_file(cfg) : filename,
			config_error_line(cfg)
		);
	}

	config_setting_t *cfg_root = config_root_setting(cfg);

	/* Parse global settings */
	if (set) {
		if (!cfg_root || !config_setting_is_group(cfg_root))
			error("Missing global section in config file: %s", filename);

		cfg_parse_global(cfg_root, set);
	}

	/* Parse nodes */
	if (nodes) {
		config_setting_t *cfg_nodes = config_setting_get_member(cfg_root, "nodes");
		if (!cfg_nodes || !config_setting_is_group(cfg_nodes))
			error("Missing node section in config file: %s", filename);

		for (int i = 0; i < config_setting_length(cfg_nodes); i++) {
			config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);
			cfg_parse_node(cfg_node, nodes, set);
		}
	}

	/* Parse paths */
	if (paths) {
		config_setting_t *cfg_paths = config_setting_get_member(cfg_root, "paths");
		if (!cfg_paths || !config_setting_is_list(cfg_paths))
			error("Missing path section in config file: %s", filename);

		for (int i = 0; i < config_setting_length(cfg_paths); i++) {
			config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);
			cfg_parse_path(cfg_path, paths, nodes, set);
		}
	}

	return 0;
}

int cfg_parse_plugins(config_setting_t *cfg)
{
	if (!config_setting_is_array(cfg))
		cerror(cfg, "Setting 'plugins' must be a list of strings");

	for (int i = 0; i < config_setting_length(cfg); i++) {
		void *handle;
		const char *path;

		path = config_setting_get_string_elem(cfg, i);
		if (!path)
			cerror(cfg, "Setting 'plugins' must be a list of strings");
		
		handle = dlopen(path, RTLD_NOW);
		if (!handle)
			error("Failed to load plugin %s", dlerror());
	}

	return 0;
}

int cfg_parse_global(config_setting_t *cfg, struct settings *set)
{
	config_setting_t *cfg_plugins;

	if (!config_setting_lookup_int(cfg, "affinity", &set->affinity))
		set->affinity = 0;

	if (!config_setting_lookup_int(cfg, "priority", &set->priority))
		set->priority = 0;

	if (!config_setting_lookup_int(cfg, "debug", &set->debug))
		set->debug = V;

	if (!config_setting_lookup_float(cfg, "stats", &set->stats))
		set->stats = 0;
	
	cfg_plugins = config_setting_get_member(cfg, "plugins");
	if (cfg_plugins)
		cfg_parse_plugins(cfg_plugins);

	log_setlevel(set->debug, -1);

	return 0;
}

int cfg_parse_path(config_setting_t *cfg,
	struct list *paths, struct list *nodes, struct settings *set)
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

int cfg_parse_node(config_setting_t *cfg, struct list *nodes, struct settings *set)
{
	const char *type, *name;
	int ret;

	struct node *n;
	struct node_type *vt;

	/* Required settings */
	if (!config_setting_lookup_string(cfg, "type", &type))
		cerror(cfg, "Missing node type");
	
	name = config_setting_name(cfg);

	vt = list_lookup(&node_types, type);
	if (!vt)
		cerror(cfg, "Invalid type for node '%s'", config_setting_name(cfg));
	
	n = node_create(vt);
	
	n->name = name;
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
