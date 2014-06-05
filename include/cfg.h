/** Configuration file parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file cfg.h
 */

#ifndef _CFG_H_
#define _CFG_H_

#include <libconfig.h>

struct node;
struct path;

/// Global configuration
struct config {
	/// Name of this node
	const char *name;
	/// Configuration filename
	const char *filename;
	/// Verbosity level
	int debug;
	/// Task priority (lower is better)
	int priority;
	/// Core affinity of this task
	int affinity;
	/// Protocol version of UDP packages
	int protocol;
	/// Number of parsed paths
	int path_count;
	/// Number of parsed nodes
	int node_count;

	/// libconfig object
	config_t obj;

	/// Array of nodes
	struct node *nodes;
	/// Array of paths
	struct path *paths;
};

/** Parse configuration file and store settings in supplied struct config.
 *
 * @param c A libconfig object
 * @param g The global configuration structure (also contains the config filename)
 */
int config_parse(config_t *c, struct config *g);

/** Parse the global section of a configuration file.
 *
 * @param c A libconfig object pointing to the root of the file
 * @param g The global configuration file
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_global(config_setting_t *c, struct config *g);

/** Parse a single path and add it to the global configuration.
  *
 * @param c A libconfig object pointing to the path
 * @param g The global configuration file
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_path(config_setting_t *c, struct config *g);

/** Parse a single node and add it to the global configuration.
  *
 * @param c A libconfig object pointing to the node
 * @param g The global configuration file
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_node(config_setting_t *c, struct config *g);

#endif /* _CFG_H_ */
