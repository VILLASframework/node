/** Node type: Client for the Universal Data-exchange API (v2)
 *
 * @see https://github.com/ERIGrid2/JRA-3.1-api
 * @author Andres Acosta <andres.acosta@eonerc.rwth-aachen.de>
 * @copyright 2014-2023, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <cstring>
#include <cstdio>

#include <unistd.h>
#include <jansson.h>
#include <pthread.h>
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <curl/curl.h>

#include <villas/api.hpp>
#include <villas/api/universal.hpp>
#include <villas/nodes/api_client.hpp>
#include <villas/utils.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>
#include <villas/timing.hpp>
#include <villas/node/config.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::api;

struct api_response {
	char *data;
	size_t len;
};

APIClientNode::APIClientNode(const uuid_t &id, const std::string &name) :
	APINode(id, name)
{
	CURLcode ret = curl_global_init(CURL_GLOBAL_ALL);

	if (ret)
		throw RuntimeError("failed to initialize CURL");

}

APIClientNode::~APIClientNode()
{
	curl_global_cleanup();
}

int APIClientNode::start()
{
	APINode::start();
	read.curl = curl_easy_init();
	write.curl = curl_easy_init();
	headers = nullptr;

	if (access_token) {
		char buf[128];
		snprintf(buf, sizeof(buf), "Auth-Token: %s", access_token);
		headers = curl_slist_append(headers, buf);
	}

	/* Create task */
	if (timeout > 1 / rate)
		logger->warn("Timeout is to large for given rate: {}", rate);

	task.setRate(rate);

	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");

	CURL *handles[] = { read.curl, write.curl };

	for (unsigned p = 0; p < ARRAY_LEN(handles); p++) {
		curl_easy_setopt(handles[p], CURLOPT_SSL_VERIFYPEER, ssl_verify);
		curl_easy_setopt(handles[p], CURLOPT_TIMEOUT_MS, timeout * 1e3);
		curl_easy_setopt(handles[p], CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(handles[p], CURLOPT_USERAGENT, HTTP_USER_AGENT);
	}

	return 0;
}

int APIClientNode::stop()
{
	// int ret;

	task.stop();

	curl_easy_cleanup(read.curl);
	curl_easy_cleanup(write.curl);
	curl_slist_free_all(headers);

	return 0;
}

int APIClientNode::reverse()
{
	swapSignals();
	SWAP(read.channels, write.channels);

	return 0;
}

int APIClientNode::parse_channel_response(json_t *json_response, int *code, char **reason, json_t **json_rvalue, Logger logger)
{
	int ret;

	double timestamp = 0;
	const char *validity = nullptr;
	const char *source = nullptr;
	const char *timesource = nullptr;

	json_error_t err;
	ret = json_unpack_ex(json_response, &err, 0, "{ s: F, s: O, s?: s, s?: s, s?: s }",
		"timestamp", &timestamp,
		"value", json_rvalue,
		"validity", &validity,
		"timesource", &timesource,
		"source", &source
	);

	if (ret) {
		logger->warn("Failed to find API response code");
		return ret;
	}

	if (validity)
		logger->warn("Attribute 'validity' is not supported by VILLASnode");

	if (source)
		logger->warn("Attribute 'source' is not supported by VILLASnode");

	if (timesource)
		logger->warn("Attribute 'timesource' is not supported by VILLASnode");

	return ret;
}

static
size_t request_writer(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct api_response *mem = (struct api_response *) userp;

	mem->data = (char *) realloc(mem->data, mem->len + realsize + 1);
	if (mem->data == nullptr) /* out of memory! */
		throw MemoryAllocationError();

	memcpy(&(mem->data[mem->len]), contents, realsize);
	mem->len += realsize;
	mem->data[mem->len] = 0;

	return realsize;
}

int APIClientNode::request_channel_query(CURL *handle, const char *endpoint, const char *signal_name, json_t **json_rvalue, Logger logger)
{
	int ret, code;
	char *reason;
	json_t *json_response;

	struct api_response chunk = { 0 };
	int old;
	double time;
	char url[128];
	json_error_t err;

	snprintf(url, sizeof(url), "%s/channel/%s/sample", endpoint, signal_name);

	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, request_writer);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) &chunk);

	logger->debug("Request to context broker: {}", url);

	/* We don't want to leave the handle in an invalid state */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
	CURLcode curl_ret = curl_easy_perform(handle);
	pthread_setcancelstate(old, nullptr);

	if (curl_ret) {
		logger->warn("HTTP request failed: {}", curl_easy_strerror(curl_ret));
		goto out;
	}

	curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &time);

	logger->debug("Request to context broker completed in {} seconds", time);
	logger->debug("Response from context broker:\n{}", chunk.data);

	json_response = json_loads(chunk.data, 0, &err);
	if (!json_response)
		logger->warn("Received invalid JSON: {} in {}:{}:{}\n{}", err.text, err.source, err.line, err.column, chunk.data);

	ret = parse_channel_response(json_response, &code, &reason, json_rvalue, logger);
	if (ret)
		goto out2;

out:	free(chunk.data);
out2:	json_decref(json_response);

	return ret;
}

int APIClientNode::_read(struct Sample *smps[], unsigned cnt)
{
	int ret;

	if (task.wait() == 0)
		throw SystemError("Failed to wait for task");

	json_t *json_rvalue;

	for (size_t i = 0; i < MIN(getInputSignals()->size(), read.channels.size()); i++) {
		auto sig = getInputSignals()->at(i);

		if (!sig)
			return -11;

		auto ch = read.channels.at(i);

		ret = request_channel_query(read.curl, endpoint, sig->name.c_str(), &json_rvalue, logger);
		if (ret)
			goto out;

		struct Sample *smp = smps[0];

		ret = smp->data[i].parseJson(sig->type, json_rvalue);

		if (ret) {
			throw BadRequest("Malformed value");
		}
	}

out:	json_decref(json_rvalue);

	return ret;
}

int APIClientNode::_write(struct Sample *smps[], unsigned cnt)
{
	// assert(cnt == 1);

	// sample_copy(write.sample, smps[0]);

	// pthread_cond_signal(&write.cv);

	return 1;
}

int APIClientNode::parse(json_t *json)
{
	int ret = Node::parse(json);
	if (ret)
		return ret;

	json_t *json_signals_in = nullptr;
	json_t *json_signals_out = nullptr;

	json_error_t err;
	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: o }, s?: { s?: o } }",
		"in",
			"signals", &json_signals_in,
		"out",
			"signals", &json_signals_out
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-api");

	if (json_signals_in)
		read.channels.parse(json_signals_in, false, true);

	if (json_signals_out)
		write.channels.parse(json_signals_out, true, false);

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s: s, s?: b, s?: F, s?: F }",
		"access_token", &access_token,
		"endpoint", &endpoint,
		"ssl_verify", &ssl_verify,
		"timeout", &timeout,
		"rate", &rate
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-api-client");

	return 0;
}

static char n[] = "api-client";
static char d[] = "Client for the HTTP REST interface";
static NodePlugin<APIClientNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE, 1> p;
