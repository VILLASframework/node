/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "utils.h"
#include "list.h"
#include "if.h"
#include "tc.h"
#include "cfg.h"
#include "node.h"
#include "path.h"
#include "hooks.h"

#include "socket.h"

#ifdef ENABLE_GTFPGA
 #include "gtfpga.h"
#endif
#ifdef ENABLE_OPAL_ASYNC
 #include "opal.h"
#endif

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
			config_parse_node(cfg_node, nodes);
		}
	}

	/* Parse paths */
	if (paths) {
		config_setting_t *cfg_paths = config_setting_get_member(cfg_root, "paths");
		if (!cfg_paths || !config_setting_is_list(cfg_paths))
			error("Missing path section in config file: %s", filename);

		for (int i = 0; i < config_setting_length(cfg_paths); i++) {
			config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);
			config_parse_path(cfg_path, paths, nodes);
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

	log_setlevel(set->debug);

	return 0;
}

int config_parse_path(config_setting_t *cfg,
	struct list *paths, struct list *nodes)
{
	const char *in;
	int enabled;
	int reverse;
	
	struct path *p = path_create();

	/* Input node */
	struct config_setting_t *cfg_in = config_setting_get_member(cfg, "in");
	if (!cfg_in || config_setting_type(cfg_in) != CONFIG_TYPE_STRING)
		cerror(cfg, "Invalid input node for path");
	
	in = config_setting_get_string(cfg_in);
	p->in = node_lookup_name(in, nodes);
	if (!p->in)
		cerror(cfg_in, "Invalid input node '%s'", in);

	/* Output node(s) */
	struct config_setting_t *cfg_out = config_setting_get_member(cfg, "out");
	if (cfg_out)
		config_parse_nodelist(cfg_out, &p->destinations, nodes);

	if (list_length(&p->destinations) >= 1)
		p->out = list_first(&p->destinations)->node;
	else
		cerror(cfg, "Missing output node for path");

	/* Optional settings */
	struct config_setting_t *cfg_hook = config_setting_get_member(cfg, "hook");
	if (cfg_hook)
		config_parse_hooks(cfg_hook, &p->hooks);
	
	if (!config_setting_lookup_bool(cfg, "enabled", &enabled))
		enabled = 1;
	if (!config_setting_lookup_bool(cfg, "reverse", &reverse))
		reverse = 0;
	if (!config_setting_lookup_float(cfg, "rate", &p->rate))
		p->rate = 0; /* disabled */
	if (!config_setting_lookup_int(cfg, "poolsize", &p->poolsize))
		p->poolsize = DEFAULT_POOLSIZE;

	p->cfg = cfg;

	if (enabled) {
		p->in->refcnt++;
		p->in->vt->refcnt++;
				
		FOREACH(&p->destinations, it) {
			it->node->refcnt++;
			it->node->vt->refcnt++;
		}
		
		if (reverse) {
			if (list_length(&p->destinations) > 1)
				error("Can't reverse path with multiple destination nodes");

			struct path *r = path_create();

			r->in  = p->out; /* Swap in/out */
			r->out = p->in;
			
			list_push(&r->destinations, r->out);

			r->in->refcnt++;
			r->out->refcnt++;
			r->in->vt->refcnt++;
			r->out->vt->refcnt++;

			list_push(paths, r);
		}
		
		list_push(paths, p);
	}
	else {
		char buf[33];
		path_print(p, buf, sizeof(buf));
		
		warn("Path %s is not enabled", buf);
		path_destroy(p);
	}

	return 0;
}

int config_parse_nodelist(config_setting_t *cfg, struct list *nodes, struct list *all) {
	const char *str;
	struct node *node;
	
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			str = config_setting_get_string(cfg);
			node = node_lookup_name(str, all);
			if (!node)
				cerror(cfg, "Invalid outgoing node '%s'", str);
				
			list_push(nodes, node);
			break;
		
		case CONFIG_TYPE_ARRAY:
			for (int i=0; i<config_setting_length(cfg); i++) {
				str = config_setting_get_string_elem(cfg, i);
				node = node_lookup_name(str, all);
				if (!node)
					cerror(config_setting_get_elem(cfg, i), "Invalid outgoing node '%s'", str);
				
				list_push(nodes, node);
			}
			break;
		
		default:
			cerror(cfg, "Invalid output node(s)");
	}
	
	return 0;
}

int config_parse_hooks(config_setting_t *cfg, struct list *hooks) {
	const char *str;
	hook_cb_t hook;
	
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			str = config_setting_get_string(cfg);
			hook = hook_lookup(str);
			if (!hook)
				cerror(cfg, "Invalid hook function '%s'", str);
				
			list_push(hooks, hook);
			break;
		
		case CONFIG_TYPE_ARRAY:
			for (int i=0; i<config_setting_length(cfg); i++) {
				str = config_setting_get_string_elem(cfg, i);
				hook = hook_lookup(str);
				if (!hook)
					cerror(config_setting_get_elem(cfg, i), "Invalid hook function '%s'", str);
				
				list_push(hooks, hook);
			}
			break;
		
		default:
			cerror(cfg, "Invalid hook functions");
	}
	
	return 0;
}

int config_parse_node(config_setting_t *cfg, struct list *nodes)
{
	const char *type;
	int ret;

	struct node *n = node_create();

	/* Required settings */
	n->cfg = cfg;
	n->name = config_setting_name(cfg);
	if (!n->name)
		cerror(cfg, "Missing node name");

	if (!config_setting_lookup_string(cfg, "type", &type))
		cerror(cfg, "Missing node type");
	
	if (!config_setting_lookup_int(cfg, "combine", &n->combine))
		n->combine = 1;
		
	n->vt = node_lookup_vtable(type);
	if (!n->vt)
		cerror(cfg, "Invalid type for node '%s'", n->name);

	ret = n->vt->parse(cfg, n);
	if (!ret)
		list_push(nodes, n);

	return ret;
}

