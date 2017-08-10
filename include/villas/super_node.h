/** Configuration file parser.
 *
 * The server program is configured by a single file.
 * This config file is parsed with a third-party library:
 *  libconfig http://www.hyperrealm.com/libconfig/
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <libconfig.h>

#include "list.h"
#include "api.h"
#include "web.h"
#include "log.h"
#include "common.h"

/** Global configuration */
struct super_node {
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
	
	char *uri;		/**< URI of configuration */

	config_t cfg;		/**< Pointer to configuration file */
	json_t *json;		/**< JSON representation of the same config. */
};

/* Compatibility with libconfig < 1.5 */
#if (LIBCONFIG_VER_MAJOR <= 1) && (LIBCONFIG_VER_MINOR < 5)
  #define config_setting_lookup config_lookup_from
#endif

/** Inititalize configuration object before parsing the configuration. */
int super_node_init(struct super_node *sn);

/** Wrapper for super_node_parse() */
int super_node_parse_cli(struct super_node *sn, int argc, char *argv[]);

/** Wrapper for super_node_parse() */
int super_node_parse_uri(struct super_node *sn, const char *uri);

/** Parse super-node configuration.
 *
 * @param sn The super-node datastructure to fill.
 * @param cfg A libconfig setting object.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int super_node_parse(struct super_node *sn, config_setting_t *cfg);

/** Check validity of super node configuration. */
int super_node_check(struct super_node *sn);

/** Initialize after parsing the configuration file. */
int super_node_start(struct super_node *sn);

int super_node_stop(struct super_node *sn);

/** Desctroy configuration object. */
int super_node_destroy(struct super_node *sn);
