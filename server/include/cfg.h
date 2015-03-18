/** Configuration file parser.
 *
 * The server program is configured by a single file.
 * This config file is parsed with a third-party library:
 *  libconfig http://www.hyperrealm.com/libconfig/
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _CFG_H_
#define _CFG_H_

#include <libconfig.h>

/* Forward declarations */
struct list;
struct node;
struct path;
struct interface;

struct socket;
struct opal;
struct gtfpga;
struct netem;

/** Global configuration */
struct settings {
	/** Process priority (lower is better) */
	int priority;
	/** Process affinity of the server and all created threads */
	int affinity;
	/** Debug log level */
	int debug;
	/** Interval for path statistics. Set to 0 to disable themo disable them. */
	double stats;

	/** A libconfig object pointing to the root of the config file */
	config_setting_t *cfg;
};

/** Parse config file and store settings in supplied struct settings.
 *
 * @param filename The path to the configration file (relative or absolute)
 * @param cfg A initialized libconfig object
 * @param set The global configuration structure
 * @param nodes A linked list of nodes which should be parsed
 * @param paths A linked list of paths which should be parsed
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct node **nodes, struct path **paths);

/** Parse the global section of a configuration file.
 *
 * @param cfg A libconfig object pointing to the root of the file
 * @param set The global configuration file
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_global(config_setting_t *cfg, struct settings *set);

/** Parse a single path and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the path
 * @param paths Add new paths to this linked list
 * @param nodes A linked list of all existing nodes
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_path(config_setting_t *cfg,
	struct path **paths, struct node **nodes);
	
int config_parse_nodelist(config_setting_t *cfg, struct list *nodes, struct node **all);


int config_parse_hooks(config_setting_t *cfg, struct list *hooks);

/** Parse a single node and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the node.
 * @param nodes Add new nodes to this linked list.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_node(config_setting_t *cfg, struct node **nodes);

/** Parse node connection details for OPAL type
 *
 * @param cfg A libconfig object pointing to the node.
 * @param nodes Add new nodes to this linked list.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_opal(config_setting_t *cfg, struct node *n);

/** Parse node connection details for GTFPGA type
 *
 * @param cfg A libconfig object pointing to the node.
 * @param n A pointer to the node structure which should be parsed.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_gtfpga(config_setting_t *cfg, struct node *n);

/** Parse node connection details for SOCKET type
 *
 * @param cfg A libconfig object pointing to the node.
 * @param n A pointer to the node structure which should be parsed.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_socket(config_setting_t *cfg, struct node *n);

/** Parse network emulator (netem) settings.
 *
 * @param cfg A libconfig object containing the settings.
 * @param em A pointer to the netem settings structure (part of the path structure).
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_netem(config_setting_t *cfg, struct netem *em);

#endif /* _CFG_H_ */
