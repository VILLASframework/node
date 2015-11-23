
/** Node type: OMA Next Generation Services Interface 10 (NGSI) (FIWARE context broker)
 *
 * This file implements the NGSI context interface. NGSI is RESTful HTTP is specified by
 * the Open Mobile Alliance (OMA).
 * It uses the standard operations of the NGSI 10 context information standard.
 *
 * @see https://forge.fiware.org/plugins/mediawiki/wiki/fiware/index.php/FI-WARE_NGSI-10_Open_RESTful_API_Specification
 * @see http://technical.openmobilealliance.org/Technical/Release_Program/docs/NGSI/V1_0-20120529-A/OMA-TS-NGSI_Context_Management-V1_0-20120529-A.pdf
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 **********************************************************************************/

#include <string.h>
#include <stdio.h>

#include <curl/curl.h>
#include <uuid/uuid.h>
#include <jansson.h>
#include <math.h>
#include <pthread.h>

#include "ngsi.h"
#include "utils.h"
#include "timing.h"

extern struct settings settings;

#if 0 /* unused at the moment */
static json_t * json_uuid()
{
	char eid[37];
	uuid_t uuid;
	
	uuid_generate_time(uuid);
	uuid_unparse_lower(uuid, eid);

	return json_string(eid);
}


static json_t * json_date(struct timespec *ts)
{
	// Example: 2015-09-21T11:42:25+02:00
	char date[64];
	strftimespec(date, sizeof(date), "%FT%T.%u%z", ts);
	
	return json_string(date);
}

static json_t * json_lookup(json_t *array, char *key, char *needle)
{
	size_t ind;
	json_t *obj;

	json_array_foreach(array, ind, obj) {
		json_t *value = json_object_get(obj, key);
		if (value && json_is_string(value)) {
			if (!strcmp(json_string_value(value), needle))
				return obj;
		}
	}
	
	return NULL;
}
#endif

enum ngsi_flags {
	NGSI_ENTITY_ATTRIBUTES = (1 << 0),
	NGSI_ENTITY_VALUES     = (1 << 1) | NGSI_ENTITY_ATTRIBUTES,
	NGSI_ENTITY_METADATA   = (1 << 2) | NGSI_ENTITY_ATTRIBUTES,
};

struct ngsi_metadata {
	char *name;
	char *type;
	char *value;
};

struct ngsi_response {
	char *data;
	size_t len;
};

static json_t* ngsi_build_entity(struct ngsi *i, struct msg *pool, int poolsize, int first, int cnt, int flags)
{
	json_t *entity = json_pack("{ s: s, s: s, s: b }",
		"id",		i->entity_id,
		"type",		i->entity_type,
		"isPattern", 	0
	);
	
	if (flags & NGSI_ENTITY_ATTRIBUTES) {
		json_t *attributes = json_array();
		list_foreach(struct ngsi_attribute *map, &i->mapping) {
			json_t *attribute = json_pack("{ s: s, s: s }",
				"name",		map->name,
				"type",		map->type
			);
			
			if (flags & NGSI_ENTITY_VALUES) { /* Build value vector */
				json_t *values = json_array();
				for (int k = 0; k < cnt; k++) {
					struct msg *m = &pool[(first + poolsize + k) % poolsize];

					json_array_append_new(values, json_pack("[ f, f, i ]",
						time_to_double(&MSG_TS(m)),
						m->data[map->index].f,
						m->sequence
					));
				}
				
				json_object_set(attribute, "value", values);
			}
			
			if (flags & NGSI_ENTITY_METADATA) { /* Create Metadata for attribute */
				json_t *metadatas = json_array();
				list_foreach(struct ngsi_metadata *meta, &map->metadata) {
					json_array_append_new(metadatas, json_pack("{ s: s, s: s, s: s }",
						"name",  meta->name,
						"type",  meta->type,
						"value", meta->value
					));
				}
				
				json_object_set(attribute, "metadatas", metadatas);
			}

			json_array_append_new(attributes, attribute);
		}
		
		json_object_set(entity, "attributes", attributes);
	}

	return entity;
}

static int ngsi_parse_entity(json_t *entity, struct ngsi *i, struct msg *pool, int poolsize, int first, int cnt)
{
	int ret;
	const char *id, *name, *type;

	size_t index;
	json_t *attribute, *attributes;

	ret = json_unpack(entity, "{ s: s, s: s, s: o }",
		"id", &id,
		"type", &type,
		"attributes", &attributes
	);
	if (ret || !json_is_array(attributes))
		return -1;
	
	if (strcmp(id, i->entity_id) || strcmp(type, i->entity_type))
		return -2;
	
	for (int j = 0; j < cnt; j++) {
		struct msg *m = &pool[(first + j) % poolsize];
		
		m->version = MSG_VERSION;
		m->length = json_array_size(attributes);
		m->endian = MSG_ENDIAN_HOST;
	}

	json_array_foreach(attributes, index, attribute) {
		struct ngsi_attribute *map;
		json_t *metadata, *values, *tuple;

		/* Parse JSON */
		ret = json_unpack(attribute, "{ s: s, s: s, s: o, s?: o }",
			"name", &name,
			"type", &type,
			"value", &values,
			"metadatas", &metadata
		);
		if (ret)
			return -3;
		
		/* Check attribute name and type */
		map = list_lookup(&i->mapping, name);
		if (!map || strcmp(map->type, type))
			return -4;
		
		/* Check metadata */
		if (!json_is_array(metadata))
			return -5;
		
		/* Check number of values */
		if (!json_is_array(values) || json_array_size(values) != cnt)
			return -6;

		size_t index2;
		json_array_foreach(values, index2, tuple) {
			struct msg *m = &pool[(first + index2) % poolsize];
			
			/* Check sample format */
			if (!json_is_array(tuple) || json_array_size(tuple) != 3)
				return -7;

			char *end;
			const char *value, *ts, *seq;
			ret = json_unpack(tuple, "[ s, s, s ]", &ts, &value, &seq);
			if (ret) 
				return -8;
			
			m->sequence = atoi(seq);
			
			struct timespec tss = time_from_double(strtod(ts, &end));
			if (ts == end)
				return -9;

			m->ts.sec  = tss.tv_sec;
			m->ts.nsec = tss.tv_nsec;

			m->data[map->index].f = strtof(value, &end);
			if (value == end)
				return -10;
		}
	}
	
	return cnt;
}

static int ngsi_parse_mapping(struct list *mapping, config_setting_t *cfg)
{
	if (!config_setting_is_array(cfg))
		return -1;

	list_init(mapping, NULL);

	for (int j = 0; j < config_setting_length(cfg); j++) { INDENT
		const char *token = config_setting_get_string_elem(cfg, j);
		if (!token)
			return -2;

		struct ngsi_attribute map = { .index = j };
		
		/* Parse Attribute: AttributeName(AttributeType) */
		int bytes;
		if (sscanf(token, "%m[^(](%m[^)])%n", &map.name, &map.type, &bytes) != 2)
			cerror(cfg, "Invalid mapping token: '%s'", token);

		token += bytes;		
		debug(13, "Attribute: %u: %s(%s)", map.index, map.name, map.type);

		/* MetadataName(MetadataType)=MetadataValue */
		list_init(&map.metadata, NULL);
		struct ngsi_metadata meta;
		while (sscanf(token, " %m[^(](%m[^)])=%ms%n", &meta.name, &meta.type, &meta.value, &bytes) == 3) { INDENT
			list_push(&map.metadata, memdup(&meta, sizeof(struct ngsi_metadata)));
			
			token += bytes;
			debug(13, "Metadata: %s(%s)=%s", meta.name, meta.type, meta.value);
		}
		
		/* Static metadata */
		struct ngsi_metadata source = {
			.name = "source",
			.type = "string",
			.value = (char *) settings.name,
		};
		
		struct ngsi_metadata index = {
			.name = "index",
			.type = "integer",
			.value = alloc(8)
		};
		snprintf(index.value, 8, "%u", j);

		list_push(&map.metadata, memdup(&index, sizeof(struct ngsi_metadata)));		
		list_push(&map.metadata, memdup(&source, sizeof(struct ngsi_metadata)));
		
		list_push(mapping, memdup(&map, sizeof(struct ngsi_attribute)));
	}

	return 0;
}

static int ngsi_parse_context_response(json_t *response, int *code, char **reason, json_t **rentity) {
	int ret;
	char *codestr;
	 
	ret = json_unpack(response, "{ s: [ { s: O, s: { s: s, s: s } } ] }",
		"contextResponses",
			"contextElement", rentity,
			"statusCode",
				"code", &codestr,
				"reasonPhrase", reason
	);			
	if (ret) {
		warn("Failed to find NGSI response code");
		return ret;
	}
	
	*code = atoi(codestr);
	
	if (*code != 200)
		warn("NGSI response: %s %s", codestr, *reason);

	return ret;
}

static size_t ngsi_request_writer(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct ngsi_response *mem = (struct ngsi_response *) userp;
 
	mem->data = realloc(mem->data, mem->len + realsize + 1);
	if(mem->data == NULL) /* out of memory! */
		error("Not enough memory (realloc returned NULL)");

	memcpy(&(mem->data[mem->len]), contents, realsize);
	mem->len += realsize;
	mem->data[mem->len] = 0;

	return realsize;
}

static int ngsi_request(CURL *handle, const char *endpoint, const char *operation, json_t *request, json_t **response)
{
	struct ngsi_response chunk = { 0 };
	char *post = json_dumps(request, JSON_INDENT(4));
	int old;
	double time;
	char url[128];
	json_error_t err;

	snprintf(url, sizeof(url), "%s/v1/%s", endpoint, operation);
	
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, ngsi_request_writer);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) &chunk);	
	curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, strlen(post));
	curl_easy_setopt(handle, CURLOPT_POSTFIELDS, post);
	
	debug(18, "Request to context broker: %s\n%s", url, post);

	/* We don't want to leave the handle in an invalid state */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
	CURLcode ret = curl_easy_perform(handle);
	pthread_setcancelstate(old, NULL);

	if (ret) {
		warn("HTTP request failed: %s", curl_easy_strerror(ret));
		goto out;
	}

	curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &time);
	
	debug(16, "Request to context broker completed in %.4f seconds", time);
	debug(17, "Response from context broker:\n%s", chunk.data);
	
	*response = json_loads(chunk.data, 0, &err);
	if (!*response)
		warn("Received invalid JSON: %s in %s:%u:%u\n%s", err.text, err.source, err.line, err.column, chunk.data);
	
out:	free(post);
	free(chunk.data);

	return ret;
}

static int ngsi_request_context_query(CURL *handle, const char *endpoint, json_t *entity, json_t **rentity)
{
	int ret, code;
	char *reason;

	json_t *response;
	json_t *request = json_pack("{ s: [ o ] }", "entities", entity);

	ret = ngsi_request(handle, endpoint, "queryContext", request, &response);
	if (ret)
		goto out;
	
	ret = ngsi_parse_context_response(response, &code, &reason, rentity);
	if (ret)
		goto out2;

out2:	json_decref(response);
out:	json_decref(request);

	return ret;
}

static int ngsi_request_context_update(CURL *handle, const char *endpoint, const char *action, json_t *entity)
{
	int ret, code;
	char *reason;
	
	json_t *response;
	json_t *request = json_pack("{ s: s, s: [ o ] }",
		"updateAction", action,
		"contextElements", entity
	);
	
	ret = ngsi_request(handle, endpoint, "updateContext", request, &response);
	if (ret)
		goto out;
	
	json_t *rentity;
	ret = ngsi_parse_context_response(response, &code, &reason, &rentity);
	if (ret)
		goto out2;

	json_decref(rentity);
out2:	json_decref(response);
out:	json_decref(request);

	return ret;
}

int ngsi_init(int argc, char *argv[], struct settings *set)
{
	return curl_global_init(CURL_GLOBAL_ALL);
}

int ngsi_deinit()
{
	curl_global_cleanup();

	return 0;
}

int ngsi_parse(struct node *n, config_setting_t *cfg)
{
	struct ngsi *i = alloc(sizeof(struct ngsi));

	if (!config_setting_lookup_string(cfg, "access_token", &i->access_token))
		i->access_token = NULL; /* disabled by default */

	if (!config_setting_lookup_string(cfg, "endpoint", &i->endpoint))
		cerror(cfg, "Missing NGSI endpoint for node '%s'", n->name);
	
	if (!config_setting_lookup_string(cfg, "entity_id", &i->entity_id))
		cerror(cfg, "Missing NGSI entity ID for node '%s'", n->name);
	
	if (!config_setting_lookup_string(cfg, "entity_type", &i->entity_type))
		cerror(cfg, "Missing NGSI entity type for node '%s'", n->name);

	if (!config_setting_lookup_bool(cfg, "ssl_verify", &i->ssl_verify))
		i->ssl_verify = 1; /* verify by default */

	if (!config_setting_lookup_float(cfg, "timeout", &i->timeout))
		i->timeout = 1; /* default value */
	
	if (!config_setting_lookup_float(cfg, "rate", &i->rate))
		i->rate = 5; /* default value */

	config_setting_t *cfg_mapping = config_setting_get_member(cfg, "mapping");
	if (!cfg_mapping)
		cerror(cfg, "Missing mapping for node '%s", n->name);

	if (ngsi_parse_mapping(&i->mapping, cfg_mapping))
		cerror(cfg_mapping, "Invalid mapping for NGSI node '%s'", n->name);

	n->ngsi = i;
	
	return 0;
}

char * ngsi_print(struct node *n)
{
	struct ngsi *i = n->ngsi;
	char *buf = NULL;

	return strcatf(&buf, "endpoint=%s, timeout=%.3f secs, #mappings=%zu",
		i->endpoint, i->timeout, list_length(&i->mapping));
}

int ngsi_destroy(struct node *n)
{
	struct ngsi *i = n->ngsi;
	
	list_destroy(&i->mapping);
	
	return 0;
}

int ngsi_open(struct node *n)
{
	struct ngsi *i = n->ngsi;
	int ret;
	
	i->curl = curl_easy_init();
	i->headers = NULL;
	
	if (i->access_token) {
		char buf[128];
		snprintf(buf, sizeof(buf), "Auth-Token: %s", i->access_token);
		i->headers = curl_slist_append(i->headers, buf);
	}
	
	/* Create timer */
	i->tfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (i->tfd < 0)
		serror("Failed to create timer");
	
	/* Arm the timer with a fixed rate */
	struct itimerspec its = {
		.it_interval = time_from_double(1 / i->rate),
		.it_value = { 0, 1 },
	};
	
	if (i->timeout > 1 / i->rate)
		warn("Timeout is to large for given rate: %f", i->rate);

	ret = timerfd_settime(i->tfd, 0, &its, NULL);
	if (ret)
		serror("Failed to start timer");
	
	i->headers = curl_slist_append(i->headers, "User-Agent: S2SS " VERSION);
	i->headers = curl_slist_append(i->headers, "Accept: application/json");
	i->headers = curl_slist_append(i->headers, "Content-Type: application/json");
	
	curl_easy_setopt(i->curl, CURLOPT_SSL_VERIFYPEER, i->ssl_verify);
	curl_easy_setopt(i->curl, CURLOPT_TIMEOUT_MS, i->timeout * 1e3);
	curl_easy_setopt(i->curl, CURLOPT_HTTPHEADER, i->headers);
		
	/* Create entity and atributes */
	json_t *entity = ngsi_build_entity(i, NULL, 0, 0, 0, NGSI_ENTITY_METADATA);	
	
	ret = ngsi_request_context_update(i->curl, i->endpoint, "APPEND", entity);
	if (ret)
		error("Failed to create NGSI context for node '%s'", n->name);
	
	json_decref(entity);
	
	return ret;
}

int ngsi_close(struct node *n)
{
	struct ngsi *i = n->ngsi;
	int ret;
	
	/* Delete complete entity (not just attributes) */
	json_t *entity = ngsi_build_entity(i, NULL, 0, 0, 0, 0);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "DELETE", entity);
		
	json_decref(entity);
	
	curl_easy_cleanup(i->curl);
	curl_slist_free_all(i->headers);
	
	return ret;
}

int ngsi_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct ngsi *i = n->ngsi;
	int ret;
	
	timerfd_wait(i->tfd);
	
	json_t *rentity;
	json_t  *entity = ngsi_build_entity(i, NULL, 0, 0, 0, 0);

	ret = ngsi_request_context_query(i->curl, i->endpoint, entity, &rentity);
	if (ret)
		goto out;
	
	ret = ngsi_parse_entity(rentity, i, pool, poolsize, first, cnt);
	if (ret)
		goto out2;
	
out2:	json_decref(rentity);
out:	json_decref(entity);

	return ret;
}

int ngsi_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct ngsi *i = n->ngsi;
	int ret;
	
	json_t *entity = ngsi_build_entity(i, pool, poolsize, first, cnt, NGSI_ENTITY_VALUES);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "UPDATE", entity);
	
	json_decref(entity);
	
	return ret ? 0 : cnt;
}

REGISTER_NODE_TYPE(NGSI, "ngsi", ngsi)
