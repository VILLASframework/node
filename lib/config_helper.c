/** Helpers for configuration parsers.
 *
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

#include "config_helper.h"
#include "utils.h"

static int json_to_config_type(int type)
{
	switch (type) {
		case JSON_OBJECT:	return CONFIG_TYPE_GROUP;
		case JSON_ARRAY:	return CONFIG_TYPE_LIST;
		case JSON_STRING:	return CONFIG_TYPE_STRING;
		case JSON_INTEGER:	return CONFIG_TYPE_INT64;
		case JSON_REAL:		return CONFIG_TYPE_FLOAT;
		case JSON_TRUE:
		case JSON_FALSE:
		case JSON_NULL:		return CONFIG_TYPE_BOOL;
	}

	return -1;
}

json_t * config_to_json(config_setting_t *cfg)
{
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_INT:	 return json_integer(config_setting_get_int(cfg));
		case CONFIG_TYPE_INT64:	 return json_integer(config_setting_get_int64(cfg));
		case CONFIG_TYPE_FLOAT:  return json_real(config_setting_get_float(cfg));
		case CONFIG_TYPE_STRING: return json_string(config_setting_get_string(cfg));
		case CONFIG_TYPE_BOOL:	 return json_boolean(config_setting_get_bool(cfg));

		case CONFIG_TYPE_ARRAY:
		case CONFIG_TYPE_LIST: {
			json_t *json = json_array();

			for (int i = 0; i < config_setting_length(cfg); i++)
				json_array_append_new(json, config_to_json(config_setting_get_elem(cfg, i)));

			return json;
		}

		case CONFIG_TYPE_GROUP: {
			json_t *json = json_object();

			for (int i = 0; i < config_setting_length(cfg); i++) {
				json_object_set_new(json,
					config_setting_name(config_setting_get_elem(cfg, i)),
					config_to_json(config_setting_get_elem(cfg, i))
				);
			}

			return json;
		}

		default:
			return json_object();
	}
}

int json_to_config(json_t *json, config_setting_t *parent)
{
	config_setting_t *cfg;
	int ret, type;

	if (config_setting_is_root(parent)) {
		if (!json_is_object(json))
			return -1; /* The root must be an object! */
	}

	switch (json_typeof(json)) {
		case JSON_OBJECT: {
			const char *key;
			json_t *json_value;

			json_object_foreach(json, key, json_value) {
				type = json_to_config_type(json_typeof(json_value));

				cfg = config_setting_add(parent, key, type);
				ret = json_to_config(json_value, cfg);
				if (ret)
					return ret;
			}
			break;
		}

		case JSON_ARRAY: {
			size_t i;
			json_t *json_value;

			json_array_foreach(json, i, json_value) {
				type = json_to_config_type(json_typeof(json_value));

				cfg = config_setting_add(parent, NULL, type);
				ret = json_to_config(json_value, cfg);
				if (ret)
					return ret;
			}
			break;
		}

		case JSON_STRING:
			config_setting_set_string(parent, json_string_value(json));
			break;

		case JSON_INTEGER:
			config_setting_set_int64(parent, json_integer_value(json));
			break;

		case JSON_REAL:
			config_setting_set_float(parent, json_real_value(json));
			break;

		case JSON_TRUE:
		case JSON_FALSE:
		case JSON_NULL:
			config_setting_set_bool(parent, json_is_true(json));
			break;
	}

	return 0;
}

int config_parse_cli(config_t *cfg, int argc, char *argv[])
{
	int ret;
	char *str = NULL;

	for (int i = 0; i < argc; i++)
		str = strcatf(&str, "%s", argv[i]);

	if (!str)
		return 0;

	config_set_auto_convert(cfg, 1);

	ret = config_read_string(cfg, str);

	free(str);

	return ret != CONFIG_TRUE;
}
