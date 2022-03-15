/** Helpers for configuration parsers.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#ifdef WITH_CONFIG
  #include <libconfig.h>
#endif

#include <jansson.h>

#include <villas/node/config.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

#ifdef WITH_CONFIG

/** Convert a libconfig object to a jansson object */
json_t * config_to_json(struct config_setting_t *cfg);

/** Convert a jansson object into a libconfig object. */
int json_to_config(json_t *json, struct config_setting_t *parent);

#endif /* WITH_CONFIG */

int json_object_extend_str(json_t *orig, const char *str);

void json_object_extend_key_value(json_t *obj, const char *key, const char *value);

void json_object_extend_key_value_token(json_t *obj, const char *key, const char *value);

/** Merge two JSON objects recursively. */
int json_object_extend(json_t *orig, json_t *merge);

json_t * json_load_cli(int argc, const char *argv[]);

} /* namespace node */
} /* namespace villas */
