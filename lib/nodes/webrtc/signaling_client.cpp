/** WebRTC signaling client
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/web.hpp>
#include <villas/exceptions.hpp>
#include <villas/nodes/webrtc/signaling_client.hpp>
#include <villas/nodes/webrtc/signaling_message.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::webrtc;

SignalingClient::SignalingClient(const std::string &srv, const std::string &sess, Web *w) :
	web(w),
	logger(logging.get("webrtc:signal"))
{
	const char *prot, *a, *p;

	memset(&info, 0, sizeof(info));

	asprintf(&uri, "%s/%s", srv.c_str(), sess.c_str());

	int ret = lws_parse_uri(uri, &prot, &a, &info.port, &p);
	if (ret)
		throw RuntimeError("Failed to parse WebSocket URI: '{}'", uri);


	asprintf(&path, "/%s", p);

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
}

SignalingClient::~SignalingClient()
{
	free(path);
	free(uri);
}

void SignalingClient::connect()
{
	info.context = web->getContext();
	info.vhost = web->getVHost();

	lws_sul_schedule(info.context, 0, &sul_helper.sul, connectStatic, 1);
}

void SignalingClient::disconnect()
{
	// TODO
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
		ret = receive(in, len);
		if (ret)
			goto do_retry;

		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
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
		logger->error("Signaling connection attempts exhaused");

	return 0;
}

int SignalingClient::writable()
{
	// Skip if we have nothing to send
	if (outgoingMessages.empty())
		return 0;

	auto msg = outgoingMessages.pop();
	auto *jsonMsg = msg.toJSON();

	char buf[LWS_PRE + 1024];
	auto len = json_dumpb(jsonMsg, buf+LWS_PRE, 1024, JSON_INDENT(2));

	auto ret = lws_write(wsi, (unsigned char *) buf, len, LWS_WRITE_TEXT);
	if (ret < 0)
		return ret;

	// Reschedule callback if there are more messages to be send
	if (!outgoingMessages.empty())
		lws_callback_on_writable(wsi);

	return 0;
}

int SignalingClient::receive(void *in, size_t len)
{
	json_error_t err;
	json_t *json = json_loadb((char *) in, len, 0, &err);
	if (!json) {
		logger->error("Failed to decode json: {} at ({}:{})", err.text, err.line, err.column);
		return -1;
	}

	auto msg = SignalingMessage(json);

	cbMessage(msg);

	return 0;
}

void SignalingClient::sendMessage(const SignalingMessage &msg)
{
	outgoingMessages.push(msg);

	web->callbackOnWritable(wsi);
}
