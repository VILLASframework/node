/** Helpers for configuration parsers.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
