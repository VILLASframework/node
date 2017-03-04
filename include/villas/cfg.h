/** Configuration file parser.
 *
 * The server program is configured by a single file.
 * This config file is parsed with a third-party library:
 *  libconfig http://www.hyperrealm.com/libconfig/
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <libconfig.h>

#include "list.h"
#include "api.h"
#include "web.h"
#include "log.h"

/** Global configuration */
struct cfg {
	int priority;		/**< Process priority (lower is better) */
	int affinity;		/**< Process affinity of the server and all created threads */
	double stats;		/**< Interval for path statistics. Set to 0 to disable them. */

	struct list nodes;
	struct list paths;
	struct list plugins;

	struct log log;
	struct api api;
	struct web web;

	config_t *cfg;		/**< Pointer to configuration file */
	json_t *json;		/**< JSON representation of the same config. */
};

/* Compatibility with libconfig < 1.5 */
#if (LIBCONFIG_VER_MAJOR <= 1) && (LIBCONFIG_VER_MINOR < 5)
  #define config_setting_lookup config_lookup_from
#endif

/** Inititalize configuration object before parsing the configuration. */
int cfg_init_pre(struct cfg *cfg);

/** Initialize after parsing the configuration file. */
int cfg_init_post(struct cfg *cfg);

int cfg_deinit(struct cfg *cfg);

/** Desctroy configuration object. */
int cfg_destroy(struct cfg *cfg);

/** Parse config file and store settings in supplied struct cfg.
 *
 * @param cfg A configuration object.
 * @param filename The path to the configration file (relative or absolute)
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int cfg_parse(struct cfg *cfg, const char *uri);

/** Parse the global section of a configuration file.
 *
 * @param cfg A libconfig object pointing to the root of the file
 * @param set The global configuration file
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int cfg_parse_global(config_setting_t *cfg, struct cfg *set);

/** Parse a single path and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the path
 * @param paths Add new paths to this linked list
 * @param nodes A linked list of all existing nodes
 * @param set The global configuration structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int cfg_parse_path(config_setting_t *cfg,
	struct list *paths, struct list *nodes, struct cfg *set);

/** Parse an array or single node and checks if they exist in the "nodes" section.
 *
 * Examples:
 *     out = [ "sintef", "scedu" ]
 *     out = "acs"
 *
 * @param cfg The libconfig object handle for "out".
 * @param nodes The nodes will be added to this list.
 * @param all This list contains all valid nodes.
 */
int cfg_parse_nodelist(config_setting_t *cfg, struct list *nodes, struct list *all);

/** Parse an array or single hook function.
 *
 * Examples:
 *     hooks = [ "print", "fir" ]
 *     hooks = "log"
 **/
int cfg_parse_hooklist(config_setting_t *cfg, struct list *hooks);

/** Parse a single hook and append it to the list.
 * A hook definition is composed of the hook name and optional parameters
 * seperated by a colon.
 *
 * Examples:
 *   "print:stdout"
 */
int cfg_parse_hook(config_setting_t *cfg, struct list *list);

/** Parse a single node and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the node.
 * @param nodes Add new nodes to this linked list.
 * @param set The global configuration structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int cfg_parse_node(config_setting_t *cfg, struct list *nodes, struct cfg *set);