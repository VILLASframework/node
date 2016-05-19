
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

static json_t* json_entity(struct ngsi *i, struct msg *pool, int poolsize, int first, int cnt)
{
	json_t *attributes = json_array();
	list_foreach(struct ngsi_mapping *map, &i->mapping) {
		/* Build value vector */
		json_t *values;
		if (cnt) {
			values = json_array();
			for (int k = 0; k < cnt; k++) {
				struct msg *m = &pool[(first + k) % poolsize];

				json_array_append_new(values, json_pack("[ f, f, i ]",
					time_to_double(&MSG_TS(m)),
					m->data[map->index].f,
					m->sequence
				));
			}
		}
		else
			values = json_string("");

		/* Create Metadata for attribute */
		json_t *metadatas = json_array();
		json_array_append_new(metadatas, json_pack("{ s: s, s: s, s: s+ }",
			"name",  "source",
			"type",  "string",
			"value", "s2ss:", settings.name
		));
		json_array_append_new(metadatas, json_pack("{ s: s, s: s, s: i }",
			"name", "index",
			"type", "integer",
			"value", map->index
		));
			
		json_t *attribute = json_pack("{ s: s, s: s, s: o, s: o }",
			"name",		map->name,
			"type",		map->type,
			"value",	values,
			"metadatas",	metadatas
		);
		json_array_append_new(attributes, attribute);
	}
	
	return json_pack("{ s: s, s: s, s: b, s: o }",
		"id",		i->entity_id,
		"type",		i->entity_type,
		"isPattern", 	0,
		"attributes",	attributes
	);
}

static int json_entity_parse(json_t *entity, struct ngsi *i, struct msg *pool, int poolsize, int first, int cnt)
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
		struct ngsi_mapping *map;
		json_t *metadata, *values, *tuple;

		/* Parse JSON */
		ret = json_unpack(attribute, "{ s: s, s: s, s: o, s: o }",
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

struct ngsi_response {
	char *data;
	size_t len;
};

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

	char url[128];
	snprintf(url, sizeof(url), "%s/v1/%s", endpoint, operation);
	
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, ngsi_request_writer);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) &chunk);	
	curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, strlen(post));
	curl_easy_setopt(handle, CURLOPT_POSTFIELDS, post);
	
	debug(18, "Request to context broker: %s\n%s", url, post);

	int old; /* We don't want to leave the CUrl handle in an invalid state */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
	CURLcode ret = curl_easy_perform(handle);
	pthread_setcancelstate(old, NULL);

	if (ret)
		error("HTTP request failed: %s", curl_easy_strerror(ret));

	double time;
	curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &time);
	
	debug(16, "Request to context broker completed in %.4f seconds", time);
	debug(17, "Response from context broker:\n%s", chunk.data);
	
	json_error_t err;
	json_t *resp = json_loads(chunk.data, 0, &err);
	if (!resp)
		error("Received invalid JSON: %s in %s:%u:%u\n%s", err.text, err.source, err.line, err.column, chunk.data);
	
	if (response)
		*response = resp;
	else
		json_decref(resp);
	
	free(post);
	free(chunk.data);

	return 0;
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

int ngsi_parse(config_setting_t *cfg, struct node *n)
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

	config_setting_t *mapping = config_setting_get_member(cfg, "mapping");
	if (!mapping || !config_setting_is_array(mapping))
		cerror(cfg, "Missing mapping for node '%s", n->name);

	list_init(&i->mapping, NULL);
	for (int j = 0; j < config_setting_length(mapping); j++) {
		const char *token = config_setting_get_string_elem(mapping, j);
		if (!token)
			cerror(mapping, "Invalid token in mapping for NGSI node '%s'", n->name);
		
		struct ngsi_mapping map = {
			.index = j
		};
		
		/* Parse token */
		if (sscanf(token, "%m[^(](%m[^)])", &map.name, &map.type) != 2)
			cerror(mapping, "Invalid mapping token: '%s'", token);
		
		list_push(&i->mapping, memdup(&map, sizeof(struct ngsi_mapping)));
	}
	
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

int ngsi_open(struct node *n)
{
	struct ngsi *i = n->ngsi;
	
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

	int ret = timerfd_settime(i->tfd, 0, &its, NULL);
	if (ret)
		serror("Failed to start timer");
	
	i->headers = curl_slist_append(i->headers, "User-Agent: S2SS " VERSION);
	i->headers = curl_slist_append(i->headers, "Accept: application/json");
	i->headers = curl_slist_append(i->headers, "Content-Type: application/json");
	
	curl_easy_setopt(i->curl, CURLOPT_SSL_VERIFYPEER, i->ssl_verify);
	curl_easy_setopt(i->curl, CURLOPT_TIMEOUT_MS, i->timeout * 1e3);
	curl_easy_setopt(i->curl, CURLOPT_HTTPHEADER, i->headers);
		
	/* Create entity and atributes */
	json_t *request = json_pack("{ s: s, s: [ o ] }",
		"updateAction", "APPEND",
		"contextElements", json_entity(i, NULL, 0, 0, 0)
	);
	
	return ngsi_request(i->curl, i->endpoint, "updateContext", request, NULL);
}

int ngsi_close(struct node *n)
{
	struct ngsi *i = n->ngsi;
	int ret;
	
	/* Delete complete entity (not just attributes) */
	json_t *request = json_pack("{ s: s, s: [ { s: s, s: s, s: b } ] }",
		"updateAction", "DELETE",
		"contextElements",
			"type",	i->entity_type,
			"id",	i->entity_id,
			"isPattern", 0
	);

	ret = ngsi_request(i->curl, i->endpoint, "updateContext", request, NULL);
	json_decref(request);
	
	curl_easy_cleanup(i->curl);
	curl_slist_free_all(i->headers);
	
	return ret;
}

int ngsi_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct ngsi *i = n->ngsi;
	int ret;
	const char *code;
	
	timerfd_wait(i->tfd);

	json_t *entity;
	json_t *response;
	json_t *request = json_pack("{ s: [ { s: s, s: s, s: b } ] }",
		"entities",
			"id", i->entity_id,
			"type", i->entity_type,
			"isPattern", 0
	);

	/* Send query to broker */
	ret = ngsi_request(i->curl, i->endpoint, "queryContext", request, &response);
	if (ret < 0) {
		warn("Failed to query data from NGSI node '%s'", n->name);
		return 0;
	}
	
	/* Parse response */
	ret = json_unpack(response, "{ s: [ { s: o, s: { s: s } } ] }",
		"contextResponses", 
			"contextElement", &entity,
			"statusCode",
				"code", &code
	);
	if (ret || strcmp(code, "200")) {
		warn("Failed to parse response from NGSI node '%s'", n->name);
		return 0;
	}

	ret = json_entity_parse(entity, i, pool, poolsize, first, cnt);
	if (ret < 0) {
		warn("Failed to parse entity from context broker response: reason=%d", ret);
		return 0;
	}

	return ret;
}

int ngsi_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct ngsi *i = n->ngsi;
	
	json_t *response;
	json_t *request = json_pack("{ s: s, s : [ o ] }",
		"updateAction", "UPDATE",
		"contextElements", json_entity(i, pool, poolsize, first, cnt)
	);

	int ret = ngsi_request(i->curl, i->endpoint, "updateContext", request, &response); json_decref(request);
	if (ret)
		error("Failed to NGSI update Context request:\n%s", json_dumps(response, JSON_INDENT(4)));

	return 1;
}

REGISTER_NODE_TYPE(NGSI, "ngsi", ngsi)
