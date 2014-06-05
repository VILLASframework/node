/**
 * Configuration parser
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _CFG_H_
#define _CFG_H_

#include <libconfig.h>

#include "path.h"
#include "node.h"

struct config {
	/// Name of this node
	const char *name;
	/// Configuration filename
	const char *filename;
	/// Verbosity level
	int debug;
	/// Task priority (lower is better)
	int nice;
	/// Core affinity of this task
	int affinity;
	/// Protocol version of UDP packages
	int protocol;
	/// Number of parsed paths
	int path_count, node_count;

	/// libconfig object
	config_t obj;

	/// Array of nodes
	struct node *nodes;
	/// Array of paths
	struct path *paths;
};

/**
 * @brief Parse configuration file and store settings in suplied struct
 *
 * @param c A libconfig object
 * @param g The global configuration structure (also contains the config filename)
 */
int config_parse(config_t *c, struct config *g);

int config_parse_global(config_setting_t *c, struct config *g);

int config_parse_path(config_setting_t *c, struct config *g);

int config_parse_node(config_setting_t *c, struct config *g);

#endif /* _CFG_H_ */
