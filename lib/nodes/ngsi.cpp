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

enum NgsiFlags {
	NGSI_ENTITY_ATTRIBUTES_IN  = (1 << 0),
	NGSI_ENTITY_ATTRIBUTES_OUT = (1 << 1),
	NGSI_ENTITY_ATTRIBUTES     = NGSI_ENTITY_ATTRIBUTES_IN | NGSI_ENTITY_ATTRIBUTES_OUT,
	NGSI_ENTITY_VALUES         = (1 << 2),
	NGSI_ENTITY_METADATA       = (1 << 3),
};

class NgsiMetadata {

public:
	NgsiMetadata(json_t *json)
	{
		parse(json);
	}

	NgsiMetadata(const std::string &nam, const std::string &typ, const std::string &val) :
		name(nam),
		type(typ),
		value(val)
	{ }

	void parse(json_t *json)
	{
		int ret;

		json_error_t err;
		const char *nam, *typ, *val;

		ret = json_unpack_ex(json, &err, 0, "{ s: s, s: s, s: s }",
			"name", &nam,
			"type", &typ,
			"value", &val
		);
		if (ret)
			throw ConfigError(json, "Failed to parse NGSI metadata");

		name = nam;
		type = typ;
		value = val;
	}

	std::string name;
	std::string type;
	std::string value;
};

class NgsiAttribute {

public:
	std::string name;
	std::string type;

	size_t index;
	std::list<NgsiMetadata> metadata;

	NgsiAttribute(json_t *json, size_t j, struct signal *s)
	{
		parse(json, j, s);
	}

	void parse(json_t *json, size_t j, struct signal *s)
	{
		int ret;

		json_error_t err;
		json_t *json_metadatas = nullptr;

		const char *nam = nullptr;
		const char *typ = nullptr;

		ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: o }",
			"ngsi_attribute_name", &nam,
			"ngsi_attribute_type", &typ,
			"ngsi_metadatas", &json_metadatas
		);
		if (ret)
			throw ConfigError(json, err, "node-config-node-ngsi", "Failed to parse NGSI attribute");

		/* Copy values from node signal, if 'ngsi_attribute' settings not provided */
		if (s && !nam)
			nam = s->name ? s->name : "";

		if (s && !typ)
			typ = s->unit ? s->unit : "";

		name = nam;
		type = typ;
		index = j;

		if (json_metadatas) {
			if (!json_is_array(json_metadatas))
				throw ConfigError(json_metadatas, "node-config-ngsi-metadata", "ngsi_metadata must be a list of objects");

			json_t *json_metadata;
			json_array_foreach(json_metadatas, j, json_metadata)
				metadata.emplace_back(json_metadata);
		}

		/* Metadata: index(integer)=j */
		metadata.emplace_back("index", "integer", fmt::format("{}", j));
	}

	json_t * build(struct sample *smps[], unsigned cnt, int flags)
	{
		json_t *json_attribute = json_pack("{ s: s, s: s }",
			"name", name.c_str(),
			"type", type.c_str()
		);

		if (flags & NGSI_ENTITY_VALUES) {
#if NGSI_VECTORS
			/* Build value vector */
			json_t *json_value = json_array();

			for (unsigned k = 0; k < cnt; k++) {
				struct sample *smp = &smps[k];

				union signal_data *sd = &smp->data[index];
				struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, index);

				json_array_append_new(json_value, json_pack("[ f, o, i ]",
					time_to_double(smp->ts.origin),
					signal_data_to_json(sd, sig),
					smp->sequence
				));
			}
#else
			struct sample *smp = smps[0];

			union signal_data *sd = &smp->data[index];
			struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, index);

			json_t *json_value = signal_data_to_json(sd, sig);
#endif

			json_object_set(json_attribute, "value", json_value);
		}

		if (flags & NGSI_ENTITY_METADATA) { /* Create Metadata for attribute */
			json_t *json_metadatas = json_array();

			for (auto &meta : metadata) {
				json_array_append_new(json_metadatas, json_pack("{ s: s, s: s, s: s }",
					"name",  meta.name.c_str(),
					"type",  meta.type.c_str(),
					"value", meta.value.c_str()
				));
			}

			json_object_set(json_attribute, "metadatas", json_metadatas);
		}

		return json_attribute;
	}
};

struct ngsi_response {
	char *data;
	size_t len;
};

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

		if (flags & NGSI_ENTITY_ATTRIBUTES_IN) {
			for (size_t j = 0; j < vlist_length(&i->in.signals); j++) {
				auto *attr = (NgsiAttribute *) vlist_at(&i->in.signals, j);

				auto *json_attr = attr->build(smps, cnt, flags);

				json_array_append_new(json_attrs, json_attr);
			}
		}

		if (flags & NGSI_ENTITY_ATTRIBUTES_OUT) {
			for (size_t j = 0; j < vlist_length(&i->out.signals); j++) {
				auto *attr = (NgsiAttribute *) vlist_at(&i->out.signals, j);

				auto *json_attr = attr->build(smps, cnt, flags);

				json_array_append_new(json_attrs, json_attr);
			}
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
		NgsiAttribute *attr;
		json_error_t err;
		json_t *json_metadata, *json_value;

		char *end;
		const char *value;

		/* Parse JSON */
		ret = json_unpack_ex(json_attr, &err, 0, "{ s: s, s: s, s: o, s?: o }",
			"name", &name,
			"type", &type,
			"value", &json_value,
			"metadatas", &json_metadata
		);
		if (ret)
			return -3;

		/* Check attribute name and type */
		attr = (NgsiAttribute *) vlist_lookup(&i->in.signals, name);
		if (!attr || attr->type != type)
			continue; /* skip unknown attributes */

		length++;

		/* Check metadata */
		if (!json_is_array(json_metadata))
			return -5;

#ifdef NGSI_VECTORS
		json_t *json_tuple;
		const char *ts, *seq;

		/* Check number of values */
		if (!json_is_array(json_value) || json_array_size(json_value) != cnt)
			return -6;

		size_t k;
		json_array_foreach(json_value, k, json_tuple) {
			struct sample *smp = smps[k];

			/* Check sample format */
			if (!json_is_array(json_tuple) || json_array_size(json_tuple) != 3)
				return -7;

			ret = json_unpack_ex(json_tuple, &err, 0, "[ s, s, s ]", &ts, &value, &seq);
			if (ret)
				return -8;

			smp->sequence = atoi(seq);

			struct timespec tss = time_from_double(strtod(ts, &end));
			if (ts == end)
				return -9;

			smp->ts.origin = tss;

			union signal_data *sd = &smp->data[attr->index];
			struct signal *sig = (struct signal *) vlist_at_safe(&n->in.signals, attr->index);
			if (!sig)
				return -11;

			if (value[0] == '\0') /* No data on Orion CB? -> Use init value */
				*sd = sig->init;
			else {
				signal_data_parse_str(sd, sig, value, &end);
				if (value == end)
					return -10;
			}
		}
#else
		struct sample *smp = smps[0];

		/* Check number of values */
		if (!json_is_string(json_value))
			return -6;

		value = json_string_value(json_value);

		union signal_data *sd = &smp->data[attr->index];
		struct signal *sig = (struct signal *) vlist_at_safe(&n->in.signals, attr->index);
		if (!sig)
			return -11;

		if (value[0] == '\0') /* No data on Orion CB? -> Use init value */
			*sd = sig->init;
		else {
			signal_data_parse_str(sd, sig, value, &end);
			if (value == end)
				return -10;
		}
#endif
	}

	for (unsigned k = 0; k < cnt; k++) {
		struct sample *smp = smps[k];

		smp->length = length;
		smp->signals = &n->in.signals;
		smp->flags = (int) SampleFlags::HAS_DATA;

#ifdef NGSI_VECTORS
		smp->flags |= (int) (SampleFlags::HAS_SEQUENCE |
		                     SampleFlags::HAS_TS_ORIGIN);
#endif
	}

	return cnt;
}

static int ngsi_parse_signals(json_t *json_signals, struct vlist *ngsi_signals, struct vlist *node_signals)
{
	size_t j;
	json_t *json_signal;

	if (!json_is_array(json_signals))
		return -1;

	json_array_foreach(json_signals, j, json_signal) {
		auto *s = (struct signal *) vlist_at_safe(node_signals, j);

		auto *a = new NgsiAttribute(json_signal, j, s);
		if (!a)
			throw MemoryAllocationError();

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
		throw MemoryAllocationError();

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

	int create = 1;
	int remove = 1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: s, s: s, s: s, s?: b, s?: F, s?: F, s?: b, s?: b, s?: { s?: o }, s?: { s?: o } }",
		"access_token", &i->access_token,
		"endpoint", &i->endpoint,
		"entity_id", &i->entity_id,
		"entity_type", &i->entity_type,
		"ssl_verify", &i->ssl_verify,
		"timeout", &i->timeout,
		"rate", &i->rate,
		"create", &create,
		"delete", &remove,
		"in",
			"signals", &json_signals_in,
		"out",
			"signals", &json_signals_out
	);
	if (ret)
		throw ConfigError(cfg, err, "node-config-node-ngsi", "Failed to parse configuration of node {}", node_name(n));

	i->create = create;
	i->remove = remove;

	if (json_signals_in) {
		ret = ngsi_parse_signals(json_signals_in, &i->in.signals, &n->in.signals);
		if (ret)
			throw ConfigError(json_signals_in, "node-config-node-ngsi-in-signals", "Invalid setting 'in.signals' of node {}", node_name(n));
	}

	if (json_signals_out) {
		ret = ngsi_parse_signals(json_signals_out, &i->out.signals, &n->out.signals);
		if (ret)
			throw ConfigError(json_signals_out, "node-config-node-ngsi-out-signals", "Invalid setting 'out.signals' of node {}", node_name(n));
	}

	return 0;
}

char * ngsi_print(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	return strf("endpoint=%s, timeout=%.3f secs",
		i->endpoint, i->timeout);
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
	if (i->create) {
		json_t *json_entity = ngsi_build_entity(n, nullptr, 0, NGSI_ENTITY_ATTRIBUTES | NGSI_ENTITY_METADATA);

		ret = ngsi_request_context_update(i->curl, i->endpoint, "APPEND", json_entity);
		if (ret)
			throw RuntimeError("Failed to create NGSI context for node {}", node_name(n));

		json_decref(json_entity);
	}

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

	json_t *json_entity = ngsi_build_entity(n, smps, cnt, NGSI_ENTITY_ATTRIBUTES_OUT | NGSI_ENTITY_VALUES);

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


	for (size_t j = 0; j < vlist_length(&i->in.signals); j++) {
		auto *attr = (NgsiAttribute *) vlist_at(&i->in.signals, j);

		delete attr;
	}

	for (size_t j = 0; j < vlist_length(&i->out.signals); j++) {
		auto *attr = (NgsiAttribute *) vlist_at(&i->out.signals, j);

		delete attr;
	}

	ret = vlist_destroy(&i->in.signals);
	if (ret)
		return ret;

	ret = vlist_destroy(&i->out.signals);
	if (ret)
		return ret;

	i->task.~Task();

	return 0;
}

int ngsi_reverse(struct node *n)
{
	struct ngsi *i = (struct ngsi *) n->_vd;

	SWAP(n->in.signals, n->out.signals);
	SWAP(i->in.signals, i->out.signals);

	return 0;
}


static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "ngsi";
	p.description		= "OMA Next Generation Services Interface 10 (libcurl, libjansson)";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
#ifdef NGSI_VECTORS
	p.node.vectorize	= 0, /* unlimited */
#else
	p.node.vectorize	= 1,
#endif
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
	p.node.reverse		= ngsi_reverse;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
