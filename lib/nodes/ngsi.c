/** Node type: OMA Next Generation Services Interface 9 (NGSI) (FIWARE context broker)
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
 **********************************************************************************/

#include <string.h>
#include <stdio.h>

#include <curl/curl.h>
#include <jansson.h>
#include <pthread.h>
#include <unistd.h>

#include <villas/nodes/ngsi.h>
#include <villas/utils.h>
#include <villas/timing.h>
#include <villas/plugin.h>
#include <villas/node/config.h>

/* Some global settings */
static char *name = NULL;

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

struct ngsi_attribute {
	char *name;
	char *type;

	int index;
	struct vlist metadata;
};

struct ngsi_response {
	char *data;
	size_t len;
};

static json_t* ngsi_build_entity(struct ngsi *i, struct sample *smps[], unsigned cnt, int flags)
{
	json_t *entity = json_pack("{ s: s, s: s, s: b }",
		"id",		i->entity_id,
		"type",		i->entity_type,
		"isPattern", 	0
	);

	if (flags & NGSI_ENTITY_ATTRIBUTES) {
		json_t *attributes = json_array();

		for (size_t j = 0; j < vlist_length(&i->mapping); j++) {
			struct ngsi_attribute *map = (struct ngsi_attribute *) vlist_at(&i->mapping, j);

			json_t *attribute = json_pack("{ s: s, s: s }",
				"name",		map->name,
				"type",		map->type
			);

			if (flags & NGSI_ENTITY_VALUES) { /* Build value vector */
				json_t *values = json_array();
				for (int k = 0; k < cnt; k++) {
					json_array_append_new(values, json_pack("[ f, f, i ]",
						time_to_double(&smps[k]->ts.origin),
						smps[k]->data[map->index].f,
						smps[k]->sequence
					));
				}

				json_object_set(attribute, "value", values);
			}

			if (flags & NGSI_ENTITY_METADATA) { /* Create Metadata for attribute */
				json_t *metadatas = json_array();

				for (size_t i = 0; i < vlist_length(&map->metadata); i++) {
					struct ngsi_metadata *meta = (struct ngsi_metadata *) vlist_at(&map->metadata, i);

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

static int ngsi_parse_entity(json_t *entity, struct ngsi *i, struct sample *smps[], unsigned cnt)
{
	int ret;
	const char *id, *name, *type;

	size_t l;
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

	for (int k = 0; k < cnt; k++)
		smps[k]->length = json_array_size(attributes);

	json_array_foreach(attributes, l, attribute) {
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
		map = vlist_lookup(&i->mapping, name);
		if (!map || strcmp(map->type, type))
			return -4;

		/* Check metadata */
		if (!json_is_array(metadata))
			return -5;

		/* Check number of values */
		if (!json_is_array(values) || json_array_size(values) != cnt)
			return -6;

		size_t k;
		json_array_foreach(values, k, tuple) {
			/* Check sample format */
			if (!json_is_array(tuple) || json_array_size(tuple) != 3)
				return -7;

			char *end;
			const char *value, *ts, *seq;
			ret = json_unpack(tuple, "[ s, s, s ]", &ts, &value, &seq);
			if (ret)
				return -8;

			smps[k]->sequence = atoi(seq);

			struct timespec tss = time_from_double(strtod(ts, &end));
			if (ts == end)
				return -9;

			smps[k]->ts.origin = tss;
			smps[k]->data[map->index].f = strtof(value, &end);
			if (value == end)
				return -10;
		}
	}

	return cnt;
}

static int ngsi_parse_mapping(struct vlist *mapping, json_t *cfg)
{
	if (!json_is_array(cfg))
		return -1;

	vlist_init(mapping);

	size_t j;
	json_t *json_token;

	json_array_foreach(cfg, j, json_token) {
		const char *token;

		token = json_string_value(json_token);
		if (!token)
			return -2;

		struct ngsi_attribute *a = (struct ngsi_attribute *) alloc(sizeof(struct ngsi_attribute));

		a->index = j;

		/* Parse Attribute: AttributeName(AttributeType) */
		int bytes;
		if (sscanf(token, "%m[^(](%m[^)])%n", &a->name, &a->type, &bytes) != 2)
			error("Invalid mapping token: '%s'", token);

		token += bytes;

		/* MetadataName(MetadataType)=MetadataValue */
		vlist_init(&a->metadata);

		struct ngsi_metadata m;
		while (sscanf(token, " %m[^(](%m[^)])=%ms%n", &m.name, &m.type, &m.value, &bytes) == 3) {
			vlist_push(&a->metadata, memdup(&m, sizeof(m)));
			token += bytes;
		}

		/* Metadata: source(string)=name */
		struct ngsi_metadata s = {
			.name = "source",
			.type = "string",
			.value = name
		};

		/* Metadata: index(integer)=j */
		struct ngsi_metadata i = {
			.name = "index",
			.type = "integer"
		};
		assert(asprintf(&i.value, "%zu", j));

		vlist_push(&a->metadata, memdup(&s, sizeof(s)));
		vlist_push(&a->metadata, memdup(&i, sizeof(i)));

		vlist_push(mapping, a);
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
		warning("Failed to find NGSI response code");
		return ret;
	}

	*code = atoi(codestr);

	if (*code != 200)
		warning("NGSI response: %s %s", codestr, *reason);

	return ret;
}

static size_t ngsi_request_writer(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct ngsi_response *mem = (struct ngsi_response *) userp;

	mem->data = realloc(mem->data, mem->len + realsize + 1);
	if (mem->data == NULL) /* out of memory! */
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

	debug(LOG_NGSI | 18, "Request to context broker: %s\n%s", url, post);

	/* We don't want to leave the handle in an invalid state */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
	CURLcode ret = curl_easy_perform(handle);
	pthread_setcancelstate(old, NULL);

	if (ret) {
		warning("HTTP request failed: %s", curl_easy_strerror(ret));
		goto out;
	}

	curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &time);

	debug(LOG_NGSI | 16, "Request to context broker completed in %.4f seconds", time);
	debug(LOG_NGSI | 17, "Response from context broker:\n%s", chunk.data);

	*response = json_loads(chunk.data, 0, &err);
	if (!*response)
		warning("Received invalid JSON: %s in %s:%u:%u\n%s", err.text, err.source, err.line, err.column, chunk.data);

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

int ngsi_type_start(struct super_node *sn)
{
	return curl_global_init(CURL_GLOBAL_ALL);
}

int ngsi_type_stop()
{
	free(name);

	curl_global_cleanup();

	return 0;
}

int ngsi_parse(struct node *n, json_t *cfg)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	int ret;
	json_error_t err;
	json_t *json_mapping;

	/* Default values */
	i->access_token = NULL; /* disabled by default */
	i->ssl_verify = 1; /* verify by default */
	i->timeout = 1; /* default value */
	i->rate = 5; /* default value */

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: s, s: s, s: s, s?: b, s?: F, s?: F }",
		"access_token", &i->access_token,
		"endpoint", &i->endpoint,
		"entity_id", &i->entity_id,
		"entity_type", &i->entity_type,
		"ssl_verify", &i->ssl_verify,
		"timeout", &i->timeout,
		"rate", &i->rate,
		"mapping", &json_mapping
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	ret = ngsi_parse_mapping(&i->mapping, json_mapping);
	if (ret)
		error("Invalid setting 'mapping' of node %s", node_name(n));

	return 0;
}

char * ngsi_print(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	return strf("endpoint=%s, timeout=%.3f secs, #mappings=%zu",
		i->endpoint, i->timeout, vlist_length(&i->mapping));
}

static int ngsi_metadata_destroy(struct ngsi_metadata *meta)
{
	free(meta->value);
	free(meta->name);
	free(meta->type);

	return 0;
}

static int ngsi_attribute_destroy(struct ngsi_attribute *attr)
{
	free(attr->name);
	free(attr->type);

	vlist_destroy(&attr->metadata, (dtor_cb_t) ngsi_metadata_destroy, true);

	return 0;
}

int ngsi_destroy(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	vlist_destroy(&i->mapping, (dtor_cb_t) ngsi_attribute_destroy, true);

	return 0;
}

int ngsi_start(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	i->curl = curl_easy_init();
	i->headers = NULL;

	if (i->access_token) {
		char buf[128];
		snprintf(buf, sizeof(buf), "Auth-Token: %s", i->access_token);
		i->headers = curl_svlist_append(i->headers, buf);
	}

	/* Create task */
	if (i->timeout > 1 / i->rate)
		warning("Timeout is to large for given rate: %f", i->rate);

	ret = task_init(&i->task, i->rate, CLOCK_MONOTONIC);
	if (ret)
		serror("Failed to create task");

	i->headers = curl_svlist_append(i->headers, "Accept: application/json");
	i->headers = curl_svlist_append(i->headers, "Content-Type: application/json");

	curl_easy_setopt(i->curl, CURLOPT_SSL_VERIFYPEER, i->ssl_verify);
	curl_easy_setopt(i->curl, CURLOPT_TIMEOUT_MS, i->timeout * 1e3);
	curl_easy_setopt(i->curl, CURLOPT_HTTPHEADER, i->headers);
	curl_easy_setopt(i->curl, CURLOPT_USERAGENT, USER_AGENT);

	/* Create entity and atributes */
	json_t *entity = ngsi_build_entity(i, NULL, 0, NGSI_ENTITY_METADATA);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "APPEND", entity);
	if (ret)
		error("Failed to create NGSI context for node %s", node_name(n));

	json_decref(entity);

	return ret;
}

int ngsi_stop(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	/* Delete complete entity (not just attributes) */
	json_t *entity = ngsi_build_entity(i, NULL, 0, 0);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "DELETE", entity);

	json_decref(entity);

	curl_easy_cleanup(i->curl);
	curl_svlist_free_all(i->headers);

	return ret;
}

int ngsi_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	if (task_wait(&i->task) == 0)
		perror("Failed to wait for task");

	json_t *rentity;
	json_t *entity = ngsi_build_entity(i, NULL, 0, 0);

	ret = ngsi_request_context_query(i->curl, i->endpoint, entity, &rentity);
	if (ret)
		goto out;

	ret = ngsi_parse_entity(rentity, i, smps, cnt);
	if (ret)
		goto out2;

out2:	json_decref(rentity);
out:	json_decref(entity);

	return ret;
}

int ngsi_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	json_t *entity = ngsi_build_entity(i, smps, cnt, NGSI_ENTITY_VALUES);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "UPDATE", entity);

	json_decref(entity);

	return ret ? 0 : cnt;
}

int ngsi_poll_fds(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	fds[0] = task_fd(&i->task);

	return 1;
}

static struct plugin p = {
	.name		= "ngsi",
	.description	= "OMA Next Generation Services Interface 10 (libcurl, libjansson)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct ngsi),
		.type.start	= ngsi_type_start,
		.type.stop	= ngsi_type_stop,
		.parse		= ngsi_parse,
		.print		= ngsi_print,
		.start		= ngsi_start,
		.stop		= ngsi_stop,
		.read		= ngsi_read,
		.write		= ngsi_write,
		.poll_fds	= ngsi_poll_fds
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
