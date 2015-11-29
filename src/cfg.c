/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "list.h"
#include "if.h"
#include "cfg.h"
#include "node.h"
#include "path.h"
#include "hooks.h"

int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct list *nodes, struct list *paths)
{
	config_set_auto_convert(cfg, 1);

	int  ret = strcmp("-", filename) ? config_read_file(cfg, filename)
					 : config_read(cfg, stdin);

	if (ret != CONFIG_TRUE)
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg),
			config_error_file(cfg) ? config_error_file(cfg) : filename,
			config_error_line(cfg)
		);

	config_setting_t *cfg_root = config_root_setting(cfg);

	/* Parse global settings */
	if (set) {
		if (!cfg_root || !config_setting_is_group(cfg_root))
			error("Missing global section in config file: %s", filename);

		config_parse_global(cfg_root, set);
	}

	/* Parse nodes */
	if (nodes) {
		config_setting_t *cfg_nodes = config_setting_get_member(cfg_root, "nodes");
		if (!cfg_nodes || !config_setting_is_group(cfg_nodes))
			error("Missing node section in config file: %s", filename);

		for (int i = 0; i < config_setting_length(cfg_nodes); i++) {
			config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);
			config_parse_node(cfg_node, nodes, set);
		}
	}

	/* Parse paths */
	if (paths) {
		config_setting_t *cfg_paths = config_setting_get_member(cfg_root, "paths");
		if (!cfg_paths || !config_setting_is_list(cfg_paths))
			error("Missing path section in config file: %s", filename);

		for (int i = 0; i < config_setting_length(cfg_paths); i++) {
			config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);
			config_parse_path(cfg_path, paths, nodes, set);
		}
	}

	return 0;
}

int config_parse_global(config_setting_t *cfg, struct settings *set)
{
	config_setting_lookup_int(cfg, "affinity", &set->affinity);
	config_setting_lookup_int(cfg, "priority", &set->priority);
	config_setting_lookup_int(cfg, "debug", &set->debug);
	config_setting_lookup_float(cfg, "stats", &set->stats);

	if (!config_setting_lookup_string(cfg, "name", &set->name)) {
		set->name = alloc(128); /** @todo missing free */
		gethostname((char *) set->name, 128);
	}

	log_setlevel(set->debug);

	return 0;
}

int config_parse_path(config_setting_t *cfg,
	struct list *paths, struct list *nodes, struct settings *set)
{
	config_setting_t *cfg_out, *cfg_hook;
	const char *in;
	int reverse;

	struct path *p = path_create();

	/* Input node */
	if (!config_setting_lookup_string(cfg, "in", &in))
		cerror(cfg, "Missing input node for path");

	p->in = list_lookup(nodes, in);
	if (!p->in)
		error("Invalid input node '%s'", in);

	/* Output node(s) */
	cfg_out = config_setting_get_member(cfg, "out");
	if (cfg_out)
		config_parse_nodelist(cfg_out, &p->destinations, nodes);
	
	p->out = (struct node *) list_first(&p->destinations);	

	/* Optional settings */
	cfg_hook = config_setting_get_member(cfg, "hook");
	if (cfg_hook)
		config_parse_hooklist(cfg_hook, &p->hooks);
	
	/* Initialize hooks and their private data / parameters */
	path_run_hook(p, HOOK_INIT);

	if (!config_setting_lookup_bool(cfg, "enabled", &p->enabled))
		p->enabled = 1;
	if (!config_setting_lookup_bool(cfg, "reverse", &reverse))
		reverse = 0;
	if (!config_setting_lookup_float(cfg, "rate", &p->rate))
		p->rate = 0; /* disabled */
	if (!config_setting_lookup_int(cfg, "poolsize", &p->poolsize))
		p->poolsize = DEFAULT_POOLSIZE;
	if (!config_setting_lookup_int(cfg, "msgsize", &p->msgsize))
		p->msgsize = MAX_VALUES;

	p->cfg = cfg;

	list_push(paths, p);

	if (reverse) {
		if (list_length(&p->destinations) > 1)
			error("Can't reverse path with multiple destination nodes");

		struct path *r = path_create();

		/* Swap in/out */
		r->in  = p->out;
		r->out = p->in;
			
		list_push(&r->destinations, r->out);
			
		if (cfg_hook)
			config_parse_hooklist(cfg_hook, &r->hooks);
			
		/* Initialize hooks and their private data / parameters */
		path_run_hook(r, HOOK_INIT);
			
		r->rate = p->rate;
		r->poolsize = p->poolsize;
		r->msgsize = p->msgsize;

		list_push(paths, r);
	}

	return 0;
}

int config_parse_nodelist(config_setting_t *cfg, struct list *list, struct list *all) {
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
					if (node)
						list_push(list, node);
					else
						cerror(elm, "Unknown outgoing node '%s'", str);
				}
				else
					cerror(cfg, "Invalid outgoing node");
			}
			break;

		default:
			cerror(cfg, "Invalid output node(s)");
	}

	return 0;
}

int config_parse_node(config_setting_t *cfg, struct list *nodes, struct settings *set)
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

	if (!config_setting_lookup_int(cfg, "combine", &n->combine))
		n->combine = 1;

	if (!config_setting_lookup_int(cfg, "affinity", &n->affinity))
		n->affinity = set->affinity;
	
	list_push(nodes, n);

	return ret;
}

int config_parse_hooklist(config_setting_t *cfg, struct list *list) {
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			config_parse_hook(cfg, list);
			break;

		case CONFIG_TYPE_ARRAY:
			for (int i = 0; i < config_setting_length(cfg); i++)
				config_parse_hook(config_setting_get_elem(cfg, i), list);
			break;

		default:
			cerror(cfg, "Invalid hook functions");
	}

	return 0;
}

int config_parse_hook(config_setting_t *cfg, struct list *list)
{
	struct hook *hook, *copy;
	const char *name = config_setting_get_string(cfg);
	if (!name)
		cerror(cfg, "Invalid hook function");
	
	char *param = strchr(name, ':');
	if (param) { /* Split hook line */
		*param = '\0';
		param++;
	}
	
	hook = list_lookup(&hooks, name);
	if (!hook)
		cerror(cfg, "Unknown hook function '%s'", name);
		
	copy = memdup(hook, sizeof(struct hook));
	copy->parameter = param;
	
	list_push(list, copy);

	return 0;
}
