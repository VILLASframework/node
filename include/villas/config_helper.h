/** Helpers for configuration parsers.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <jansson.h>

#ifdef LIBCONFIG_FOUND
#include <libconfig.h>
#endif /* LIBCONFIG_FOUND */

#include <villas/sample.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LIBCONFIG_FOUND
/* Convert a libconfig object to a jansson object */
json_t *config_to_json(config_setting_t *cfg);

/* Convert a jansson object into a libconfig object. */
int json_to_config(json_t *json, config_setting_t *parent);
#endif /* LIBCONFIG_FOUND */

/* Create a JSON object from command line parameters. */
json_t *json_load_cli(int argc, const char *argv[]);

int json_object_extend_str(json_t *orig, const char *str);

void json_object_extend_key_value(json_t *obj, const char *key, const char *value);

/* Merge two JSON objects recursively. */
int json_object_extend(json_t *orig, json_t *merge);

#ifdef __cplusplus
}
#endif
