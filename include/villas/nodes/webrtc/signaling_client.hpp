/** WebRTC signaling client
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @copyright 2023, OPAL-RT Germany GmbH
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <functional>
#include <chrono>

#include <libwebsockets.h>

#include <villas/queue.hpp>
#include <villas/buffer.hpp>
#include <villas/web.hpp>
#include <villas/log.hpp>
#include <villas/nodes/webrtc/signaling_message.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Web;

namespace webrtc {

class SignalingClient {

protected:
	struct sul_offsetof_helper {
		lws_sorted_usec_list_t sul;	/**> Schedule connection retry */
		SignalingClient *self;
	} sul_helper;

	uint16_t retry_count;			/**> Count of consecutive retries */

	struct lws *wsi;
	struct lws_client_connect_info info;

	/* The retry and backoff policy we want to use for our client connections */
	static constexpr uint32_t backoff_ms[] = { 1<<4, 1<<6, 1<<8, 1<<10, 1<<12, 1<<14, 1<<16 };
	static constexpr lws_retry_bo_t retry = {
		.retry_ms_table			= backoff_ms,
		.retry_ms_table_count		= LWS_ARRAY_SIZE(backoff_ms),
		.conceal_count			= LWS_ARRAY_SIZE(backoff_ms) + 1,

		.secs_since_valid_ping		= 3,  /* force PINGs after secs idle */
		.secs_since_valid_hangup	= 10, /* hangup after secs idle */

		.jitter_percent			= 20,
	};

	std::function<void(SignalingMessage)> cbMessage;
	std::function<void()> cbConnected;
	std::function<void()> cbDisconnected;
	std::function<void(std::string)> cbError;

	Queue<SignalingMessage> outgoingMessages;

	Web *web;

	char *uri;
	char *path;

	std::atomic<bool> running;

	Buffer buffer; // A buffer for received fragments before JSON decoding.

	Logger logger;

	int protocolCallback(struct lws *wsi, enum lws_callback_reasons reason, void *in, size_t len);

	static
	void connectStatic(struct lws_sorted_usec_list *sul);

	int writable();

public:
	SignalingClient(const std::string &server, const std::string &session, const std::string &peer, Web *w);
	~SignalingClient();

	static
	int protocolCallbackStatic(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

	void connect();
	void disconnect();

	void sendMessage(SignalingMessage);

	void onMessage(std::function<void(SignalingMessage)> callback)
	{
		cbMessage = callback;
	}

	void onConnected(std::function<void()> callback)
	{
		cbConnected = callback;
	}

	void onDisconnected(std::function<void()> callback)
	{
		cbDisconnected = callback;
	}

	void onError(std::function<void(std::string)> callback)
	{
		cbError = callback;
	}
};

} /* namespace webrtc */
} /* namespace node */
} /* namespace villas */

