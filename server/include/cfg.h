/** Configuration file parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _CFG_H_
#define _CFG_H_

#include <libconfig.h>

struct node;
struct path;
struct interface;

struct netem;

/** Global configuration */
struct settings {
	/** Task priority (lower is better) */
	int priority;
	/** Core affinity of this task */
	int affinity;

	/** A libconfig object pointing to the root of the config file */
	config_setting_t *cfg;
};

/** Parse configuration file and store settings in supplied struct settings.
 *
 * @param filename The path to the configration file (relative or absolute)
 * @param cfg A initialized libconfig object
 * @param set The global configuration structure
 * @param nodes A linked list of nodes which should be parsed
 * @param paths A linked list of paths which should be parsed
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct node **nodes, struct path **paths);

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
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_node(config_setting_t *cfg,
	struct node **nodes);

/** Parse network emulator (netem) settings
 *
 * @param cfg A libconfig object containing the settings
 * @param em A pointer to the settings
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int config_parse_netem(config_setting_t *cfg, struct netem *em);

#endif /* _CFG_H_ */
