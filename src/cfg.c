/**
 * Configuration parser
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "cfg.h"
#include "node.h"
#include "path.h"
#include "utils.h"

int config_parse(const char *filename, config_t *cfg,
	struct settings *g, struct node **n, struct path **p)
{
	if (!config_read_file(cfg, filename)) {
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg),
			config_error_file(cfg),
			config_error_line(cfg)
		);
	}

	/* Get and check config sections */
	config_setting_t *cfg_root = config_root_setting(cfg);
	if (!cfg_root || !config_setting_is_group(cfg_root))
		error("Missing global section in config file: %s", filename);

	config_setting_t *cfg_nodes = config_setting_get_member(cfg_root, "nodes");
	if (!cfg_nodes || !config_setting_is_group(cfg_nodes))
		error("Missing node section in config file: %s", filename);

	config_setting_t *cfg_paths = config_setting_get_member(cfg_root, "paths");
	if (!cfg_paths || !config_setting_is_list(cfg_paths))
		error("Missing path section in config file: %s", filename);

	/* Parse global settings */
	config_parse_global(cfg_root, g);

	/* Parse nodes */
	for (int i = 0; i < config_setting_length(cfg_nodes); i++) {
		config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);

		struct node *node = (struct node *) malloc(sizeof(struct node));
		if (!node)
			error("Failed to allocate memory for node");

		if (!config_parse_node(cfg_node, node)) {
			free(node);
			cerror(cfg_node, "Failed to parse node");
		}

		list_add(*n, node);
	}

	/* Parse paths */
	for (int i = 0; i < config_setting_length(cfg_paths); i++) {
		config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);

		struct path *path = (struct path *) malloc(sizeof(struct path));
		if (!path)
			error("Failed to allocate memory for path");

		if (!config_parse_path(cfg_path, path, *n)) {
			free(path);
			cerror(cfg_path, "Failed to parse path");
		}

		list_add(*p, path);
	}

	return CONFIG_TRUE;
}

int config_parse_global(config_setting_t *cfg, struct settings *set)
{
	if (!config_setting_lookup_string(cfg, "name", &set->name))
		cerror(cfg, "Missing node name");

	config_setting_lookup_int(cfg, "affinity", &set->affinity);
	config_setting_lookup_int(cfg, "priority", &set->priority);
	config_setting_lookup_int(cfg, "protocol", &set->protocol);

	set->cfg = cfg;

	return CONFIG_TRUE;
}

int config_parse_path(config_setting_t *cfg,
	struct path *path, struct node *nodes)
{
	struct node *in, *out;
	const char *in_str = NULL;
	const char *out_str = NULL;
	int enabled = 1;
	int reverse = 0;

	/* Optional settings */
	config_setting_lookup_bool(cfg, "enabled", &enabled);
	config_setting_lookup_bool(cfg, "reverse", &reverse);

	/* Required settings */
	if (!config_setting_lookup_string(cfg, "in", &in_str))
		cerror(cfg, "Missing input node for path");

	if (!config_setting_lookup_string(cfg, "out", &out_str))
		cerror(cfg, "Missing output node for path");

	info("Loading path from '%s' to '%s'", in_str, out_str);

	in = node_lookup_name(in_str, nodes);
	if (!in)
		cerror(cfg, "Invalid input node '%s'");

	out = node_lookup_name(out_str, nodes);
	if (!out)
		cerror(cfg, "Invalid output node '%s'", out_str);

	if (enabled) {
		if (path_create(path, in, out))
			cerror(cfg, "Failed to parse path");

		path->cfg = cfg;

		// TODO
		/*if (reverse) {
			if (path_create(path, out, in))
				cerror(c, "Failed to parse path");
		}*/
	}
	else
		warn("  Path is not enabled");
}

int config_parse_node(config_setting_t *cfg, struct node *node)
{
	const char *name = NULL;
	const char *type_str = NULL;
	const char *remote_str = NULL;
	const char *local_str = NULL;

	struct sockaddr_in local, remote;
	enum node_type type;

	/* Required settings */
	name = config_setting_name(cfg);
	if (!name)
		cerror(cfg, "Missing node name");

	if (!config_setting_lookup_string(cfg, "type", &type_str))
		cerror(cfg, "Missing node type");

	if (!config_setting_lookup_string(cfg, "remote", &remote_str))
		cerror(cfg, "Missing node remote address");

	if (!config_setting_lookup_string(cfg, "local", &local_str))
		cerror(cfg, "Missing node local address");

	type = node_lookup_type(type_str);
	if (type == NODE_INVALID)
		cerror(cfg, "Invalid node type '%s'", type);

	info("Loading %s node '%s'", type_str, name);

	if (resolve_addr(local_str, &local, 0))
		cerror(cfg, "Failed to resolve local address '%s' of node '%s'", local_str, name);

	if (resolve_addr(remote_str, &remote, 0))
		cerror(cfg, "Failed to resolve remote address '%s' of node '%s'", remote_str, name);

	if (node_create(node, name, type, local, remote))
		cerror(cfg, "Failed to parse node");

	node->cfg = cfg;
}
