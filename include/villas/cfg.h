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
#include "common.h"

/** Global configuration */
struct cfg {
	int priority;		/**< Process priority (lower is better) */
	int affinity;		/**< Process affinity of the server and all created threads */
	int hugepages;		/**< Number of hugepages to reserve. */
	double stats;		/**< Interval for path statistics. Set to 0 to disable them. */

	struct list nodes;
	struct list paths;
	struct list plugins;

	struct log log;
	struct api api;
	struct web web;
	
	struct {
		int argc;
		char **argv;
	} cli;
	
	enum state state;

	config_t cfg;		/**< Pointer to configuration file */
	json_t *json;		/**< JSON representation of the same config. */
};

/* Compatibility with libconfig < 1.5 */
#if (LIBCONFIG_VER_MAJOR <= 1) && (LIBCONFIG_VER_MINOR < 5)
  #define config_setting_lookup config_lookup_from
#endif

/** Inititalize configuration object before parsing the configuration. */
int cfg_init(struct cfg *cfg);

/** Initialize after parsing the configuration file. */
int cfg_start(struct cfg *cfg);

int cfg_stop(struct cfg *cfg);

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

int cfg_parse_cli(struct cfg *cfg, int argc, char *argv[]);