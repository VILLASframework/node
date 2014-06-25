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
struct interface;

/** Global configuration */
struct settings {
	/** Name of this node */
	const char *name;
	/** Task priority (lower is better) */
	int priority;
	/** Core affinity of this task */
	int affinity;
	/** Protocol version of UDP packages */
	int protocol;
	/** User for the server process */
	int uid;
	/** Group for the server process */
	int gid;

	/** A libconfig object pointing to the root of the config file */
	config_setting_t *cfg;
};

/** Parse configuration file and store settings in supplied struct settings.
 *
 * @param cfg A libconfig object
 * @param set The global configuration structure (also contains the config filename)
 * @param nodes A pointer to a linked list of nodes which should be parsed
 * @param paths A pointer to a linked list of paths which should be parsed
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct node **nodes, struct path **paths, struct interface **interfaces);

/** Parse the global section of a configuration file.
 *
 * @param cfg A libconfig object pointing to the root of the file
 * @param set The global configuration file
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_global(config_setting_t *cfg, struct settings *set);

/** Parse a single path and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the path
 * @param paths Add new paths to this linked list
 * @param nodes A linked list of all existing nodes
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_path(config_setting_t *cfg,
	struct path **paths, struct node **nodes);

/** Parse a single node and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the node
 * @param nodes Add new nodes to this linked list
 * @param interfaces Search this list for existing interfaces
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_node(config_setting_t *cfg,
	struct node **nodes, struct interface **interfaces);
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */

#endif /* _CFG_H_ */
