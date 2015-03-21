/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
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
#include "gtfpga.h"
#ifdef ENABLE_OPAL_ASYNC
#include "opal.h"
#endif

int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct node **nodes, struct path **paths)
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

	_debug = set->debug;

	set->cfg = cfg;

	return 0;
}

int config_parse_path(config_setting_t *cfg,
	struct path **paths, struct node **nodes)
{
	const char *in;
	int enabled = 1;
	int reverse = 0;
	
	struct path *p = alloc(sizeof(struct path));

	/* Input node */
	struct config_setting_t *cfg_in = config_setting_get_member(cfg, "in");
	if (!cfg_in || config_setting_type(cfg_in) != CONFIG_TYPE_STRING)
		cerror(cfg, "Invalid input node for path");
	
	in = config_setting_get_string(cfg_in);
	p->in = node_lookup_name(in, *nodes);
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
	
	config_setting_lookup_bool(cfg, "enabled", &enabled);
	config_setting_lookup_bool(cfg, "reverse", &reverse);
	config_setting_lookup_float(cfg, "rate", &p->rate);

	p->cfg = cfg;

	if (enabled) {
		p->in->refcnt++;
		FOREACH(&p->destinations, it)
			it->node->refcnt++;
		
		if (reverse) {
			if (list_length(&p->destinations) > 1)
				warn("Using first destination '%s' as source for reverse path. "
					"Ignoring remaining nodes", p->out->name);

			struct path *r = alloc(sizeof(struct path));

			r->in  = p->out; /* Swap in/out */
			r->out = p->in;
			
			list_push(&r->destinations, r->out);

			r->in->refcnt++;
			r->out->refcnt++;

			list_add(*paths, r);
		}
		
		list_add(*paths, p);
	}
	else {
		char buf[33];
		path_print(p, buf, sizeof(buf));
		
		warn("Path %s is not enabled", buf);
		path_destroy(p);
	}

	return 0;
}

int config_parse_nodelist(config_setting_t *cfg, struct list *nodes, struct node **all) {
	const char *str;
	struct node *node;
	
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			str = config_setting_get_string(cfg);
			node = node_lookup_name(str, *all);
			if (!node)
				cerror(cfg, "Invalid outgoing node '%s'", str);
				
			list_push(nodes, node);
			break;
		
		case CONFIG_TYPE_ARRAY:
			for (int i=0; i<config_setting_length(cfg); i++) {
				str = config_setting_get_string_elem(cfg, i);
				node = node_lookup_name(str, *all);
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

int config_parse_node(config_setting_t *cfg, struct node **nodes)
{
	const char *type;
	int ret;

	struct node *n = alloc(sizeof(struct node));

	/* Required settings */
	n->cfg = cfg;
	n->name = config_setting_name(cfg);
	if (!n->name)
		cerror(cfg, "Missing node name");

	if (config_setting_lookup_string(cfg, "type", &type)) {
		n->vt = node_lookup_vtable(type);
		if (!n->vt)
			cerror(cfg, "Invalid type for node '%s'", n->name);

		if (!n->vt->parse)
			cerror(cfg, "Node type '%s' is not allowed in the config", type);
	}
	else
		n->vt = node_lookup_vtable("udp");

	ret = n->vt->parse(cfg, n);

	if (!ret)
		list_add(*nodes, n);

	return ret;
}

#ifdef ENABLE_OPAL_ASYNC
/** @todo: Remove this global variable. */
extern struct opal_global *og;

int config_parse_opal(config_setting_t *cfg, struct node *n)
{	
	if (!og) {
		warn("Skipping node '%s', because this server is not running as an OPAL Async process!", n->name);
		return -1;
	}
	
	struct opal *o = (struct opal *) malloc(sizeof(struct opal));
	if (!o)
		error("Failed to allocate memory for opal settings");
	
	memset(o, 0, sizeof(struct opal));
	
	config_setting_lookup_int(cfg, "send_id", &o->send_id);
	config_setting_lookup_int(cfg, "recv_id", &o->recv_id);
	config_setting_lookup_bool(cfg, "reply", &o->reply);
		
	/* Search for valid send and recv ids */
	int sfound = 0, rfound = 0;
	for (int i=0; i<og->send_icons; i++)
		sfound += og->send_ids[i] == o->send_id;
	for (int i=0; i<og->send_icons; i++)
		rfound += og->send_ids[i] == o->send_id;
	
	if (!sfound)
		cerror(config_setting_get_member(cfg, "send_id"), "Invalid send_id '%u' for node '%s'", o->send_id, n->name);
	if (!rfound)
		cerror(config_setting_get_member(cfg, "recv_id"), "Invalid recv_id '%u' for node '%s'", o->recv_id, n->name);

	n->opal = o;
	n->opal->global = og;
	n->cfg = cfg;

	return 0;
}
#endif /* ENABLE_OPAL_ASYNC */


#ifdef ENABLE_GTFPGA
/** @todo Implement */
int config_parse_gtfpga(config_setting_t *cfg, struct node *n)
{
	return 0;
}
#endif /* ENABLE_GTFPGA */

int config_parse_socket(config_setting_t *cfg, struct node *n)
{
	const char *local, *remote;
	int ret;
	
	struct socket *s = alloc(sizeof(struct socket));

	if (!config_setting_lookup_string(cfg, "remote", &remote))
		cerror(cfg, "Missing remote address for node '%s'", n->name);

	if (!config_setting_lookup_string(cfg, "local", &local))
		cerror(cfg, "Missing local address for node '%s'", n->name);

	ret = socket_parse_addr(local, (struct sockaddr *) &s->local, node_type(n), AI_PASSIVE);
	if (ret)
		cerror(cfg, "Failed to resolve local address '%s' of node '%s': %s",
			local, n->name, gai_strerror(ret));

	ret = socket_parse_addr(remote, (struct sockaddr *) &s->remote, node_type(n), 0);
	if (ret)
		cerror(cfg, "Failed to resolve remote address '%s' of node '%s': %s",
			remote, n->name, gai_strerror(ret));

	/** @todo Netem settings are not usable AF_UNIX */
	config_setting_t *cfg_netem = config_setting_get_member(cfg, "netem");
	if (cfg_netem) {
		s->netem = (struct netem *) alloc(sizeof(struct netem));
			
		config_parse_netem(cfg_netem, s->netem);
	}
	
	n->socket = s;

	return 0;
}

int config_parse_netem(config_setting_t *cfg, struct netem *em)
{
	em->valid = 0;

	if (config_setting_lookup_string(cfg, "distribution", &em->distribution))
		em->valid |= TC_NETEM_DISTR;
	if (config_setting_lookup_int(cfg, "delay", &em->delay))
		em->valid |= TC_NETEM_DELAY;
	if (config_setting_lookup_int(cfg, "jitter", &em->jitter))
		em->valid |= TC_NETEM_JITTER;
	if (config_setting_lookup_int(cfg, "loss", &em->loss))
		em->valid |= TC_NETEM_LOSS;
	if (config_setting_lookup_int(cfg, "duplicate", &em->duplicate))
		em->valid |= TC_NETEM_DUPL;
	if (config_setting_lookup_int(cfg, "corrupt", &em->corrupt))
		em->valid |= TC_NETEM_CORRUPT;

	/** @todo Validate netem config values */

	return 0;
}
