/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "if.h"
#include "tc.h"
#include "cfg.h"
#include "node.h"
#include "path.h"
#include "utils.h"
#include "hooks.h"

#include "socket.h"
#include "gtfpga.h"
#include "opal.h"

int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct node **nodes, struct path **paths)
{
	config_set_auto_convert(cfg, 1);

	FILE *file = (strcmp("-", filename)) ? fopen(filename, "r") : stdin;
	if (!file)
		error("Failed to open configuration file: %s", filename);

	if (!config_read(cfg, file))
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg), filename,
			config_error_line(cfg)
		);

	if (file != stdin)
		fclose(file);

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

	return CONFIG_TRUE;
}

int config_parse_global(config_setting_t *cfg, struct settings *set)
{
	config_setting_lookup_int(cfg, "affinity", &set->affinity);
	config_setting_lookup_int(cfg, "priority", &set->priority);
	config_setting_lookup_int(cfg, "debug", &set->debug);
	config_setting_lookup_float(cfg, "stats", &set->stats);

	_debug = set->debug;

	set->cfg = cfg;

	return CONFIG_TRUE;
}

int config_parse_path(config_setting_t *cfg,
	struct path **paths, struct node **nodes)
{
	const char *in, *out, *hook;
	int enabled = 1;
	int reverse = 0;

	struct path *p = alloc(sizeof(struct path));

	/* Required settings */
	if (!config_setting_lookup_string(cfg, "in", &in))
		cerror(cfg, "Missing input node for path");

	if (!config_setting_lookup_string(cfg, "out", &out))
		cerror(cfg, "Missing output node for path");

	p->in = node_lookup_name(in, *nodes);
	if (!p->in)
		cerror(cfg, "Invalid input node '%s'", in);

	p->out = node_lookup_name(out, *nodes);
	if (!p->out)
		cerror(cfg, "Invalid output node '%s'", out);

	/* Optional settings */
	if (config_setting_lookup_string(cfg, "hook", &hook)) {
		p->hook = hook_lookup(hook);
		
		if (!p->hook)
			cerror(cfg, "Failed to lookup hook function. Not registred?");
	}
	
	config_setting_lookup_bool(cfg, "enabled", &enabled);
	config_setting_lookup_bool(cfg, "reverse", &reverse);
	config_setting_lookup_float(cfg, "rate", &p->rate);

	p->cfg = cfg;

	if (enabled) {
		p->in->refcnt++;
		p->out->refcnt++;

		list_add(*paths, p);

		if (reverse) {
			struct path *rev = alloc(sizeof(struct path));

			rev->in  = p->out; /* Swap in/out */
			rev->out = p->in;

			rev->in->refcnt++;
			rev->out->refcnt++;

			list_add(*paths, rev);
		}
	}
	else {
		free(p);
		char buf[33];
		path_print(p, buf, sizeof(buf));
		
		warn("Path %s is not enabled", buf);
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
	}
	else
		n->vt = node_lookup_vtable("udp");

	ret = n->vt->parse(cfg, n);

	list_add(*nodes, n);

	return ret;
}

/** @todo Implement */
int config_parse_opal(config_setting_t *cfg, struct node *n)
{
	return 0;
}

/** @todo Implement */
int config_parse_gtfpga(config_setting_t *cfg, struct node *n)
{
	return 0;
}

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

	return CONFIG_TRUE;
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

	return CONFIG_TRUE;
}
