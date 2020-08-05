/** Node type: OMA Next Generation Services Interface 9 (NGSI) (FIWARE context broker)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstring>
#include <cstdio>

#include <curl/curl.h>
#include <jansson.h>
#include <pthread.h>
#include <unistd.h>

#include <villas/nodes/ngsi.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/timing.h>
#include <villas/plugin.h>
#include <villas/node/config.h>

using namespace villas;
using namespace villas::utils;

/* Some global settings */
static char *name = nullptr;

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

static json_t * ngsi_build_attribute(struct ngsi_attribute *attr, struct sample *smps[], unsigned cnt, int flags)
{
	json_t *json_attribute = json_pack("{ s: s, s: s }",
		"name", attr->name,
		"type", attr->type
	);

	if (flags & NGSI_ENTITY_VALUES) { /* Build value vector */
		json_t *json_values = json_array();

		for (unsigned k = 0; k < cnt; k++) {
			json_array_append_new(json_values, json_pack("[ f, f, i ]",
				time_to_double(&smps[k]->ts.origin),
				smps[k]->data[attr->index].f,
				smps[k]->sequence
			));
		}

		json_object_set(json_attribute, "value", json_values);
	}

	if (flags & NGSI_ENTITY_METADATA) { /* Create Metadata for attribute */
		json_t *json_metadatas = json_array();

		for (size_t i = 0; i < vlist_length(&attr->metadata); i++) {
			struct ngsi_metadata *meta = (struct ngsi_metadata *) vlist_at(&attr->metadata, i);

			json_array_append_new(json_metadatas, json_pack("{ s: s, s: s, s: s }",
				"name",  meta->name,
				"type",  meta->type,
				"value", meta->value
			));
		}

		json_object_set(json_attribute, "metadatas", json_metadatas);
	}

	return json_attribute;
}

static json_t* ngsi_build_entity(struct node *n, struct sample *smps[], unsigned cnt, int flags)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	json_t *json_entity = json_pack("{ s: s, s: s, s: b }",
		"id", i->entity_id,
		"type", i->entity_type,
		"isPattern", 0
	);

	if (flags & NGSI_ENTITY_ATTRIBUTES) {
		json_t *json_attrs = json_array();

		for (size_t j = 0; j < vlist_length(&i->in.signals); j++) {
			struct ngsi_attribute *attr = (struct ngsi_attribute *) vlist_at(&i->in.signals, j);

			auto *json_attr = ngsi_build_attribute(attr, smps, cnt, flags);

			json_array_append_new(json_attrs, json_attr);
		}

		for (size_t j = 0; j < vlist_length(&i->out.signals); j++) {
			struct ngsi_attribute *attr = (struct ngsi_attribute *) vlist_at(&i->out.signals, j);

			auto *json_attr = ngsi_build_attribute(attr, smps, cnt, flags);

			json_array_append_new(json_attrs, json_attr);
		}

		json_object_set(json_entity, "attributes", json_attrs);
	}

	return json_entity;
}

static int ngsi_parse_entity(struct node *n, json_t *json_entity, struct sample *smps[], unsigned cnt)
{
	int ret, length = 0;
	const char *id, *name, *type;

	struct ngsi *i = (struct ngsi *) n->_vd;

	size_t l;
	json_error_t err;
	json_t *json_attr, *json_attrs;

	ret = json_unpack_ex(json_entity, &err, 0, "{ s: s, s: s, s: o }",
		"id", &id,
		"type", &type,
		"attributes", &json_attrs
	);
	if (ret || !json_is_array(json_attrs))
		return -1;

	if (strcmp(id, i->entity_id) || strcmp(type, i->entity_type))
		return -2;

	json_array_foreach(json_attrs, l, json_attr) {
		struct ngsi_attribute *attr;
		json_error_t err;
		json_t *json_metadata, *json_values, *json_tuple;

		/* Parse JSON */
		ret = json_unpack_ex(json_attr, &err, 0, "{ s: s, s: s, s: o, s?: o }",
			"name", &name,
			"type", &type,
			"value", &json_values,
			"metadatas", &json_metadata
		);
		if (ret)
			return -3;

		/* Check attribute name and type */
		attr = (struct ngsi_attribute *) vlist_lookup(&i->in.signals, name);
		if (!attr || strcmp(attr->type, type))
			continue; /* skip unknown attributes */

		length++;

		/* Check metadata */
		if (!json_is_array(json_metadata))
			return -5;

		/* Check number of values */
		if (!json_is_array(json_values) || json_array_size(json_values) != cnt)
			return -6;

		size_t k;
		json_array_foreach(json_values, k, json_tuple) {
			/* Check sample format */
			if (!json_is_array(json_tuple) || json_array_size(json_tuple) != 3)
				return -7;

			char *end;
			const char *value, *ts, *seq;
			ret = json_unpack_ex(json_tuple, &err, 0, "[ s, s, s ]", &ts, &value, &seq);
			if (ret)
				return -8;

			smps[k]->sequence = atoi(seq);

			struct timespec tss = time_from_double(strtod(ts, &end));
			if (ts == end)
				return -9;

			smps[k]->ts.origin = tss;
			smps[k]->data[attr->index].f = strtof(value, &end);
			if (value == end)
				return -10;
		}
	}

	for (unsigned k = 0; k < cnt; k++) {
		smps[k]->length = length;
		smps[k]->signals = &n->in.signals;
	}

	return cnt;
}

static int ngsi_parse_signals(json_t *json_signals, struct vlist *ngsi_signals, struct vlist *node_signals)
{
	int ret;
	size_t j;
	json_error_t err;
	json_t *json_signal, *json_metadata;

	if (!json_is_array(json_signals))
		return -1;

	json_array_foreach(json_signals, j, json_signal) {
		auto *s = (struct signal *) vlist_at_safe(node_signals, j);

		auto *a = new struct ngsi_attribute;
		if (!a)
			throw MemoryAllocationError();

		memset(a, 0, sizeof(struct ngsi_attribute));

		a->index = j;
	
		json_t *json_ngsi_metadata = nullptr;

		ret = json_unpack_ex(json_signal, &err, 0, "{ s?: s, s?: s, s?: o }",
			"ngsi_attribute_name", &a->name,
			"ngsi_attribute_type", &a->type,
			"ngsi_metadatas", &json_ngsi_metadata
		);
		if (ret)
			return ret;

		/* Copy values from node signal, if 'ngsi_attribute' settings not provided */
		if (s != nullptr && a->name == nullptr)
			a->name = strdup(s->name ? s->name : "");

		if (s != nullptr && a->type == nullptr)
			a->type = strdup(s->unit ? s->unit : "");

		ret = vlist_init(&a->metadata);
		if (ret)
			return ret;

		if (json_ngsi_metadata) {
			if (!json_is_array(json_ngsi_metadata))
				throw ConfigError(json_ngsi_metadata, "node-config-ngsi-metadata", "ngsi_metadata must be a list of objects");

			json_array_foreach(json_ngsi_metadata, j, json_metadata) {
				auto *md = new struct ngsi_metadata;
				if (!md)
					return -1;

				memset(md, 0, sizeof(struct ngsi_metadata));

				ret = json_unpack_ex(json_metadata, &err, 0, "{ s: s, s: s, s: s }",
					"name", &md->name,
					"type", &md->type,
					"value", &md->value
				);
				if (ret)
					return ret;
				
				vlist_push(&a->metadata, md);
			}
		}

		/* Metadata: index(integer)=j */
		auto *md_idx = new struct ngsi_metadata;
		if (!md_idx)
			return -1;

		md_idx->name = strdup("index");
		md_idx->type = strdup("integer");
		asprintf(&md_idx->value, "%zu", j);

		assert(md_idx->name && md_idx->type && md_idx->value);

		vlist_push(&a->metadata, md_idx);

		vlist_push(ngsi_signals, a);
	}

	return 0;
}

static int ngsi_parse_context_response(json_t *json_response, int *code, char **reason, json_t **json_rentity) {
	int ret;
	char *codestr;

	json_error_t err;

	ret = json_unpack_ex(json_response, &err, 0, "{ s: [ { s: O, s: { s: s, s: s } } ] }",
		"contextResponses",
			"contextElement", json_rentity,
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

	mem->data = (char *) realloc(mem->data, mem->len + realsize + 1);
	if (mem->data == nullptr) /* out of memory! */
		error("Not enough memory (realloc returned nullptr)");

	memcpy(&(mem->data[mem->len]), contents, realsize);
	mem->len += realsize;
	mem->data[mem->len] = 0;

	return realsize;
}

static int ngsi_request(CURL *handle, const char *endpoint, const char *operation, json_t *json_request, json_t **json_response)
{
	struct ngsi_response chunk = { 0 };
	char *post = json_dumps(json_request, JSON_INDENT(4));
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
	pthread_setcancelstate(old, nullptr);

	if (ret) {
		warning("HTTP request failed: %s", curl_easy_strerror(ret));
		goto out;
	}

	curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &time);

	debug(LOG_NGSI | 16, "Request to context broker completed in %.4f seconds", time);
	debug(LOG_NGSI | 17, "Response from context broker:\n%s", chunk.data);

	*json_response = json_loads(chunk.data, 0, &err);
	if (!*json_response)
		warning("Received invalid JSON: %s in %s:%u:%u\n%s", err.text, err.source, err.line, err.column, chunk.data);

out:	free(post);
	free(chunk.data);

	return ret;
}

static int ngsi_request_context_query(CURL *handle, const char *endpoint, json_t *json_entity, json_t **json_rentity)
{
	int ret, code;
	char *reason;

	json_t *json_response;
	json_t *json_request = json_pack("{ s: [ o ] }", "entities", json_entity);

	ret = ngsi_request(handle, endpoint, "queryContext", json_request, &json_response);
	if (ret)
		goto out;

	ret = ngsi_parse_context_response(json_response, &code, &reason, json_rentity);
	if (ret)
		goto out2;

out2:	json_decref(json_response);
out:	json_decref(json_request);

	return ret;
}

static int ngsi_request_context_update(CURL *handle, const char *endpoint, const char *action, json_t *json_entity)
{
	int ret, code;
	char *reason;

	json_t *json_response;
	json_t *json_request = json_pack("{ s: s, s: [ o ] }",
		"updateAction", action,
		"contextElements", json_entity
	);

	ret = ngsi_request(handle, endpoint, "updateContext", json_request, &json_response);
	if (ret)
		goto out;

	json_t *json_rentity;
	ret = ngsi_parse_context_response(json_response, &code, &reason, &json_rentity);
	if (ret)
		goto out2;

	json_decref(json_rentity);
out2:	json_decref(json_response);
out:	json_decref(json_request);

	return ret;
}

int ngsi_type_start(villas::node::SuperNode *sn)
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
	json_t *json_signals_in = nullptr;
	json_t *json_signals_out = nullptr;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: s, s: s, s: s, s?: b, s?: F, s?: F, s?: { s?: o }, s?: { s?: o } }",
		"access_token", &i->access_token,
		"endpoint", &i->endpoint,
		"entity_id", &i->entity_id,
		"entity_type", &i->entity_type,
		"ssl_verify", &i->ssl_verify,
		"timeout", &i->timeout,
		"rate", &i->rate,
		"in",
			"signals", &json_signals_in,
		"out",
			"signals", &json_signals_out
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (json_signals_in) {
		ret = ngsi_parse_signals(json_signals_in, &i->in.signals, &n->in.signals);
		if (ret)
			error("Invalid setting 'in.signals' of node %s", node_name(n));
	}

	if (json_signals_out) {
		ret = ngsi_parse_signals(json_signals_out, &i->out.signals, &n->out.signals);
		if (ret)
			error("Invalid setting 'out.signals' of node %s", node_name(n));
	}

	return 0;
}

char * ngsi_print(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	return strf("endpoint=%s, timeout=%.3f secs",
		i->endpoint, i->timeout);
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

int ngsi_start(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	i->curl = curl_easy_init();
	i->headers = nullptr;

	if (i->access_token) {
		char buf[128];
		snprintf(buf, sizeof(buf), "Auth-Token: %s", i->access_token);
		i->headers = curl_slist_append(i->headers, buf);
	}

	/* Create task */
	if (i->timeout > 1 / i->rate)
		warning("Timeout is to large for given rate: %f", i->rate);

	i->task.setRate(i->rate);

	i->headers = curl_slist_append(i->headers, "Accept: application/json");
	i->headers = curl_slist_append(i->headers, "Content-Type: application/json");

	curl_easy_setopt(i->curl, CURLOPT_SSL_VERIFYPEER, i->ssl_verify);
	curl_easy_setopt(i->curl, CURLOPT_TIMEOUT_MS, i->timeout * 1e3);
	curl_easy_setopt(i->curl, CURLOPT_HTTPHEADER, i->headers);
	curl_easy_setopt(i->curl, CURLOPT_USERAGENT, USER_AGENT);

	/* Create entity and atributes */
	json_t *json_entity = ngsi_build_entity(n, nullptr, 0, NGSI_ENTITY_METADATA);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "APPEND", json_entity);
	if (ret)
		error("Failed to create NGSI context for node %s", node_name(n));

	json_decref(json_entity);

	return ret;
}

int ngsi_stop(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	i->task.stop();

	/* Delete complete entity (not just attributes) */
	json_t *json_entity = ngsi_build_entity(n, nullptr, 0, 0);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "DELETE", json_entity);

	json_decref(json_entity);

	curl_easy_cleanup(i->curl);
	curl_slist_free_all(i->headers);

	return ret;
}

int ngsi_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	if (i->task.wait() == 0)
		perror("Failed to wait for task");

	json_t *json_rentity;
	json_t *json_entity = ngsi_build_entity(n, nullptr, 0, 0);

	ret = ngsi_request_context_query(i->curl, i->endpoint, json_entity, &json_rentity);
	if (ret)
		goto out;

	ret = ngsi_parse_entity(n, json_rentity, smps, cnt);
	if (ret)
		goto out2;

out2:	json_decref(json_rentity);
out:	json_decref(json_entity);

	return ret;
}

int ngsi_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct ngsi *i = (struct ngsi *) n->_vd;
	int ret;

	json_t *json_entity = ngsi_build_entity(n, smps, cnt, NGSI_ENTITY_VALUES);

	ret = ngsi_request_context_update(i->curl, i->endpoint, "UPDATE", json_entity);

	json_decref(json_entity);

	return ret ? 0 : cnt;
}

int ngsi_poll_fds(struct node *n, int fds[])
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	fds[0] = i->task.getFD();

	return 1;
}

int ngsi_init(struct node *n)
{
	int ret;
	struct ngsi *i = (struct ngsi *) n->_vd;

	new (&i->task) Task(CLOCK_REALTIME);

	ret = vlist_init(&i->in.signals);
	if (ret)
		return ret;

	ret = vlist_init(&i->out.signals);
	if (ret)
		return ret;

	/* Default values */
	i->access_token = nullptr; /* disabled by default */
	i->ssl_verify = 1; /* verify by default */
	i->timeout = 1; /* default value */
	i->rate = 1; /* default value */

	return 0;
}

int ngsi_destroy(struct node *n)
{
	int ret;
	struct ngsi *i = (struct ngsi *) n->_vd;

	ret = vlist_destroy(&i->in.signals, (dtor_cb_t) ngsi_attribute_destroy, true);
	if (ret)
		return ret;

	ret = vlist_destroy(&i->out.signals, (dtor_cb_t) ngsi_attribute_destroy, true);
	if (ret)
		return ret;

	i->task.~Task();

	return 0;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "ngsi";
	p.description		= "OMA Next Generation Services Interface 10 (libcurl, libjansson)";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0, /* unlimited */
	p.node.size		= sizeof(struct ngsi);
	p.node.type.start	= ngsi_type_start;
	p.node.type.stop	= ngsi_type_stop;
	p.node.init		= ngsi_init;
	p.node.destroy		= ngsi_destroy;
	p.node.parse		= ngsi_parse;
	p.node.print		= ngsi_print;
	p.node.start		= ngsi_start;
	p.node.stop		= ngsi_stop;
	p.node.read		= ngsi_read;
	p.node.write		= ngsi_write;
	p.node.poll_fds		= ngsi_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
