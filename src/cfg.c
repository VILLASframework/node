/**
 * Configuration parser
 *
 * @author Steffen Vogel <steffen.vogel@rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>

#include "config.h"
#include "utils.h"

int config_parse(config_t *cfg, const char *filename, struct path *paths, struct node *nodes)
{
	if (!config_read_file(cfg, filename)) {
		logger(ERROR, "Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg), config_error_file(cfg), config_error_line(cfg));
		exit(EXIT_FAILURE);
	}

	config_setting_t *cfg_root = config_root_setting(cfg);

	// read global settings
	int debug;
	if (config_setting_lookup_int(cfg_root, "debug", &debug))
		logger(DEBUG, "Setting debug level to %u", debug);

	// read nodes
	config_setting_t *cfg_nodes = config_setting_get_member(cfg_root, "nodes");
	if (cfg_nodes) {
		for (int i=0; i < config_setting_length(cfg_nodes); i++) {
			config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);
			config_parse_node(cfg_node, nodes);
		}
	}

	// read paths
	config_setting_t *cfg_paths = config_setting_get_member(cfg_root, "paths");
	if (cfg_paths) {
		for (int i=0; i < config_setting_length(cfg_paths); i++) {
			for (int i=0; i < config_setting_length(cfg_paths); i++) {
				config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);
				config_parse_path(cfg_path, &paths[i]);
			}
		}
	}

	return CONFIG_TRUE;
}

int config_parse_path(config_setting_t *c, struct path *p)
{

	return CONFIG_TRUE;
}

int config_parse_node(config_setting_t *c, struct node *n)
{
	config_setting_lookup_string(c, "name", (const char **) &n->name);


	logger(DEBUG, "Found node: %s", n->name);

	return CONFIG_TRUE;
}

