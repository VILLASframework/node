#include "json.h"

#ifdef WITH_JANSSON
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

int sample_io_json_pack(json_t **j, struct sample *s, int flags)
{
	json_error_t err;
	json_t *json_data = json_array();
	
	for (int i = 0; i < s->length; i++) {
		json_t *json_value = sample_get_data_format(s, i)
					? json_integer(s->data[i].i)
					: json_real(s->data[i].f);
						
		json_array_append(json_data, json_value);
	}
	
	*j = json_pack_ex(&err, 0, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] }, s: I, s: o }",
		"ts",
			"origin", s->ts.origin.tv_sec, s->ts.origin.tv_nsec,
			"received", s->ts.received.tv_sec, s->ts.received.tv_nsec,
			"sent", s->ts.sent.tv_sec, s->ts.sent.tv_nsec,
		"sequence", s->sequence,
		"data", json_data);
		
	if (!*j)
		return -1;
	
	return 0;
}

int sample_io_json_unpack(json_t *j, struct sample *s, int *flags)
{
	int ret, i;
	json_t *json_data, *json_value;
	
	ret = json_unpack(j, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] }, s: I, s: o }",
		"ts",
			"origin", &s->ts.origin.tv_sec, &s->ts.origin.tv_nsec,
			"received", &s->ts.received.tv_sec, &s->ts.received.tv_nsec,
			"sent", &s->ts.sent.tv_sec, &s->ts.sent.tv_nsec,
		"sequence", &s->sequence,
		"data", &json_data);
		
	if (ret)
		return ret;
	
	s->length = 0;

	json_array_foreach(json_data, i, json_value) {
		switch (json_typeof(json_value)) {
			case JSON_REAL:
				s->data[i].f = json_real_value(json_value);
				sample_set_data_format(s, i, SAMPLE_DATA_FORMAT_FLOAT);
				break;

			case JSON_INTEGER:
				s->data[i].f = json_integer_value(json_value);
				sample_set_data_format(s, i, SAMPLE_DATA_FORMAT_INT);
				break;

			default:
				return -1;
		}
		
		s->length++;
	}

	return 0;
}

int sample_io_json_fprint(FILE *f, struct sample *s, int flags)
{
	int ret;
	json_t *json;
	
	ret = sample_io_json_pack(&json, s, flags);
	if (ret)
		return ret;
	
	ret = json_dumpf(json, f, 0);
	
	json_decref(json);
	
	return ret;
}

int sample_io_json_fscan(FILE *f, struct sample *s, int *flags)
{
	int ret;
	json_t *json;
	json_error_t err;

	json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
	if (!json)
		return -1;
	
	ret = sample_io_json_unpack(json, s, flags);
	
	json_decref(json);
	
	return ret;
}
#endif

