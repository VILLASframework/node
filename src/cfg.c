/**
 * Configuration parser
 *
 * @author Steffen Vogel <steffen.vogel@rwth-aachen.de>
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

int config_parse(config_t *c, struct config *g)
{
	config_setting_t *cfg_nodes, *cfg_paths, *cfg_root;

	if (!config_read_file(cfg, g->filename)) {
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg),
			config_error_file(cfg),
			config_error_line(cfg)
		);
	}

	cfg_root = config_root_setting(c);

	/* Read global settings */
	config_parse_global(cfg_root, g);

	/* Allocate memory for paths and nodes */
	cfg_nodes = config_setting_get_member(cfg_root, "nodes");
	if (!cfg_nodes || !config_setting_is_group(cfg_nodes))
		error("Missing node section in config");

	cfg_paths = config_setting_get_member(cfg_root, "paths");
	if (!cfg_paths || !config_setting_is_list(cfg_paths))
		error("Missing path section in config");

	int node_count = config_setting_length(cfg_nodes);
	int path_count = config_setting_length(cfg_paths);

	g->nodes = malloc(node_count * sizeof(struct node));
	if (!g->nodes)
		error("Failed to allocate memory for nodes");

	g->paths = malloc(path_count * sizeof(struct path));
	if (!g->paths)
		error("Failed to allocate memory for paths");


	/* Parse nodes */
	for (int i=0; i < node_count; i++) {
		config_parse_node(config_setting_get_elem(cfg_nodes, i), g);
	}

	/* Parse paths */
	for (int i=0; i < path_count; i++) {
		config_parse_path(config_setting_get_elem(cfg_paths, i), g);
	}

	return CONFIG_TRUE;
}

int config_parse_global(config_setting_t *c, struct config *g)
{
	if (!config_setting_lookup_string(c, "name", &g->name))
		cerror(c, "Missing node name");

	config_setting_lookup_int(c, "debug", &g->debug);
	config_setting_lookup_int(c, "affinity", &g->affinity);
	config_setting_lookup_int(c, "priority", &g->priority);
	config_setting_lookup_int(c, "protocol", &g->protocol);

	return CONFIG_TRUE;
}

int config_parse_path(config_setting_t *c, struct config *g)
{
	struct node *in, *out;
	const char *in_str = NULL;
	const char *out_str = NULL;
	int enabled = 1;
	int reverse = 0;

	/* Optional settings */
	config_setting_lookup_bool(c, "enabled", &enabled);
	config_setting_lookup_bool(c, "reverse", &reverse);

	/* Required settings */
	if (!config_setting_lookup_string(c, "in", &in_str))
		cerror(c, "Missing input node for path");

	if (!config_setting_lookup_string(c, "out", &out_str))
		cerror(c, "Missing output node for path");

	info("Loading path from '%s' to '%s'", in_str, out_str);

	in = node_lookup_name(in_str, g->nodes, g->node_count);
	if (!in)
		cerror(c, "Invalid input node '%s'");

	out = node_lookup_name(out_str, g->nodes, g->node_count);
	if (!out)
		cerror(c, "Invalid output node '%s'", out_str);

	if (enabled) {
		if (path_create(&g->paths[g->path_count], in, out))
			cerror(c, "Failed to parse path");

		g->path_count++;

		if (reverse) {
			if (path_create(&g->paths[g->path_count], out, in))
				cerror(c, "Failed to parse path");

			g->path_count++;
		}
	}
	else
		warn("  Path is not enabled");
}

int config_parse_node(config_setting_t *c, struct config *g)
{
	const char *name = NULL;
	const char *type_str = NULL;
	const char *remote_str = NULL;
	const char *local_str = NULL;

	struct sockaddr_in local;
	struct sockaddr_in remote;
	enum node_type type;

	/* Optional settings */

	/* Required settings */
	name = config_setting_name(c);
	if (!name)
		cerror(c, "Missing node name");

	if (!config_setting_lookup_string(c, "type", &type_str))
		cerror(c, "Missing node type");

	if (!config_setting_lookup_string(c, "remote", &remote_str))
		cerror(c, "Missing node remote address");

	if (!config_setting_lookup_string(c, "local", &local_str))
		cerror(c, "Missing node local address");

	type = node_lookup_type(type_str);
	if (type == NODE_INVALID)
		cerror(c, "Invalid node type '%s'", type);


	info("Loading %s node '%s'", type_str, name);

	if (resolve(local_str, &local, 0))
		cerror(c, "Failed to resolve local address '%s' of node '%s'", local_str, name);

	if (resolve(remote_str, &remote, 0))
		cerror(c, "Failed to resolve remote address '%s' of node '%s'", remote_str, name);

	if (node_create(&g->nodes[g->node_count], name, type, local, remote))
		cerror(c, "Failed to parse node");

	g->node_count++;
}

