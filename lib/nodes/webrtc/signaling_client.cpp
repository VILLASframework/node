/* WebRTC signaling client
 *
 * Author: Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fmt/format.h>

#include <villas/utils.hpp>
#include <villas/web.hpp>
#include <villas/exceptions.hpp>
#include <villas/nodes/webrtc/signaling_client.hpp>
#include <villas/nodes/webrtc/signaling_message.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::node;
using namespace villas::node::webrtc;

SignalingClient::SignalingClient(const std::string &server, const std::string &session, const std::string &peer, Web *w) :
	retry_count(0),
	web(w),
	running(false),
	logger(logging.get("webrtc:signal"))
{
	int ret;
	const char *prot, *a, *p;

	memset(&info, 0, sizeof(info));

	ret = asprintf(&uri, "%s/%s/%s", server.c_str(), session.c_str(), peer.c_str());
	if (ret < 0)
		throw RuntimeError { "Could not format signaling server uri" };

	ret = lws_parse_uri(uri, &prot, &a, &info.port, &p);
	if (ret)
		throw RuntimeError("Failed to parse WebSocket URI: '{}'", uri);

	ret = asprintf(&path, "/%s", p);
	if (ret < 0)
		throw RuntimeError { "Could not format signaling client path" };

	info.ssl_connection = !strcmp(prot, "https");
	info.address = a;
	info.path = path;
	info.host = a;
	info.origin = a;
	info.protocol = "webrtc-signaling";
	info.local_protocol_name = "webrtc-signaling";
	info.pwsi = &wsi;
	info.retry_and_idle_policy = &retry;
	info.ietf_version_or_minus_one = -1;
	info.userdata = this;

	sul_helper.self = this;
	sul_helper.sul = {};
}

SignalingClient::~SignalingClient()
{
	disconnect();

	free(path);
	free(uri);
}

void SignalingClient::connect()
{
	running = true;

	info.context = web->getContext();

	lws_sul_schedule(info.context, 0, &sul_helper.sul, connectStatic, 1 * LWS_US_PER_SEC);
}

void SignalingClient::disconnect()
{
	running = false;
	// TODO:
	// - wait for connectStatic to exit
	// - close LWS connection
	if (wsi)
		lws_callback_on_writable(wsi);
}

void SignalingClient::connectStatic(struct lws_sorted_usec_list *sul)
{
	auto *sh = lws_container_of(sul, struct sul_offsetof_helper, sul);
	auto *c = sh->self;

	if (!lws_client_connect_via_info(&c->info)) {
		/* Failed... schedule a retry... we can't use the _retry_wsi()
		 * convenience wrapper api here because no valid wsi at this
		 * point.
		 */
		if (lws_retry_sul_schedule(c->info.context, 0, sul, nullptr, connectStatic, &c->retry_count))
			c->logger->error("Signaling connection attempts exhausted");
	}
}

int SignalingClient::protocolCallbackStatic(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	auto *c = reinterpret_cast<SignalingClient *>(user);

	return c->protocolCallback(wsi, reason, in, len);
}

int SignalingClient::protocolCallback(struct lws *wsi, enum lws_callback_reasons reason, void *in, size_t len)
{
	int ret;

	switch (reason) {
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		cbError(in ? (char *) in : "unknown error");
		goto do_retry;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		if (lws_is_first_fragment(wsi))
			buffer.clear();

		buffer.append((char *) in, len);

		if (lws_is_final_fragment(wsi)) {
			logger->trace("Signaling message received: {:.{}}", buffer.data(), buffer.size());

			auto *json = buffer.decode();
			if (json == nullptr) {
				logger->error("Failed to decode JSON");
				goto do_retry;
			}

			cbMessage(SignalingMessage::fromJson(json));

			json_decref(json);
		}

		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		retry_count = 0;
		cbConnected();
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		cbDisconnected();
		goto do_retry;

	case LWS_CALLBACK_CLIENT_WRITEABLE: {
		ret = writable();
		if (ret)
			goto do_retry;

		break;
	}

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, this, in, len);

do_retry:
	logger->info("Attempting to reconnect...");

	/* Retry the connection to keep it nailed up
	 *
	 * For this example, we try to conceal any problem for one set of
	 * backoff retries and then exit the app.
	 *
	 * If you set retry.conceal_count to be larger than the number of
	 * elements in the backoff table, it will never give up and keep
	 * retrying at the last backoff delay plus the random jitter amount.
	 */
	if (lws_retry_sul_schedule_retry_wsi(wsi, &sul_helper.sul, connectStatic, &retry_count))
		logger->error("Signaling connection attempts exhausted");

	return -1;
}

int SignalingClient::writable()
{
	if (!running) {
		auto reason = "Signaling Client Closing";
		lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY, (unsigned char *) reason, strlen(reason));
		return 0;
	}

	// Skip if we have nothing to send
	if (outgoingMessages.empty()) {
		return 0;
	}

	auto msg = outgoingMessages.pop();
	auto *jsonMsg = msg.toJson();

	if (!jsonMsg) {
		return 0;
	}

	char buf[LWS_PRE + 4096];
	auto len = json_dumpb(jsonMsg, buf + LWS_PRE, 4096, JSON_INDENT(2));
	if (len > sizeof(buf) - LWS_PRE)
		return -1;

	logger->trace("Signaling message send: {:.{}}", buf + LWS_PRE, len);

	auto ret = lws_write(wsi, (unsigned char *) buf + LWS_PRE, len, LWS_WRITE_TEXT);
	if (ret < 0)
		return ret;

	// Reschedule callback if there are more messages to be send
	if (!outgoingMessages.empty())
		lws_callback_on_writable(wsi);

	return 0;
}

void SignalingClient::sendMessage(SignalingMessage msg)
{
	outgoingMessages.push(msg);

	web->callbackOnWritable(wsi);
}
