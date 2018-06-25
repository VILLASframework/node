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

#include <string.h>

#include <villas/config.h>
#include <villas/config_helper.h>
#include <villas/utils.h>

#ifdef LIBCONFIG_FOUND

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
#endif /* LIBCONFIG_FOUND */

void json_object_extend_key_value(json_t *obj, const char *key, const char *value)
{
	char *end, *cpy, *key1, *key2;

	double real;
	long integer;

	json_t *arr, *new, *existing, *subobj;

	/* Is the key pointing to an object? */
	subobj = obj;
	cpy = strdup(key);

	key1 = strtok(cpy, ".");
	key2 = strtok(NULL, ".");

	while (key1 && key2) {
		existing = json_object_get(subobj, key1);
		if (existing)
			subobj = existing;
		else {
			new = json_object();
			json_object_set(subobj, key1, new);

			subobj = new;
		}

		key1 = key2;
		key2 = strtok(NULL, ".");
 	}

	/* Try to parse as integer */
	integer = strtol(value, &end, 0);
	if (*end == 0) {
		new = json_integer(integer);
		goto success;
	}

	/* Try to parse as floating point */
	real = strtod(value, &end);
	if (*end == 0) {
		new = json_real(real);
		goto success;
	}

	/* Try to parse special types */
	if (!strcmp(value, "true")) {
		new = json_true();
		goto success;
	}

	if (!strcmp(value, "false")) {
		new = json_false();
		goto success;
	}

	if (!strcmp(value, "null")) {
		new = json_null();
		goto success;
	}

	/* Fallback to string */
	new = json_string(value);

success:
	/* Does the key already exist?
 	* If yes, transform to array. */
 	existing = json_object_get(subobj, key1);
 	if (existing) {
		if (json_is_array(existing))
			arr = existing;
		else {
			arr = json_array();
			json_object_set(subobj, key1, arr);
			json_array_append(arr, existing);
		}

		json_array_append(arr, new);
	}
	else
		json_object_set(subobj, key1, new);
}

json_t * json_load_cli(int argc, char *argv[])
{
	char *opt;
	char *key = NULL;
	char *value = NULL;
	char *sep, *cpy;

	json_t *json = json_object();

	for (int i = 1; i < argc; i++) {
		opt = argv[i];

		/* Long Option */
		if (opt[0] == '-' && opt[1] == '-') {
			/* Option without value? Abort. */
			if (key != NULL)
				return NULL;

			key = opt + 2;

			/* Does this option has the form "--option=value"? */
			sep = strchr(key, '=');
			if (sep) {
				cpy = strdup(key);

				key = strtok(cpy, "=");
				value = strtok(NULL, "");

				json_object_extend_key_value(json, key, value);

				free(cpy);
				key = NULL;
			}
		}
		/* Value */
		else {
			/* Value without key. Abort. */
			if (key == NULL)
				return NULL;

			value = opt;

			json_object_extend_key_value(json, key, value);
			key = NULL;
		}
	}

	return json;
}

int json_object_extend(json_t *obj, json_t *merge)
{
	const char *key;
	int ret;
	json_t *merge_value, *obj_value;

	if (!json_is_object(obj) || !json_is_object(merge))
		return -1;

	json_object_foreach(merge, key, merge_value) {
		obj_value = json_object_get(obj, key);
		if (obj_value && json_is_object(obj_value))
			ret = json_object_extend(obj_value, merge_value);
		else
			ret = json_object_set(obj, key, merge_value);

		if (ret)
			return ret;
	}

	return 0;
}

int json_object_extend_str(json_t *obj, const char *str)
{
	char *key, *value, *cpy;

	cpy = strdup(str);

	key = strtok(cpy, "=");
	value = strtok(NULL, "");

	if (!key || !value)
		return -1;

	json_object_extend_key_value(obj, key, value);

	free(cpy);

	return 0;
}
