/** LWS-releated functions.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <atomic>
#include <thread>

#include <libwebsockets.h>
#include <jansson.h>

#include <villas/log.hpp>
#include <villas/common.hpp>
#include <villas/queue.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Api;

class Web final {

private:
	enum State state;

	Logger logger;

	lws_context *context;		/**< The libwebsockets server context. */
	lws_vhost *vhost;		/**< The libwebsockets vhost. */

	Queue<lws *> writables;		/**< Queue of WSIs for which we will call lws_callback_on_writable() */

	int port;			/**< Port of the build in HTTP / WebSocket server. */
	std::string ssl_cert;		/**< Path to the SSL certitifcate for HTTPS / WSS. */
	std::string ssl_private_key;	/**< Path to the SSL private key for HTTPS / WSS. */

	std::thread thread;
	std::atomic<bool> running;	/**< Atomic flag for signalizing thread termination. */

	Api *api;

	void worker();

public:

	/** Initialize the web interface.
 	 *
 	 * The web interface is based on the libwebsockets library.
 	 */
	Web(Api *a = nullptr);

	~Web();

	void start();
	void stop();

	static void lwsLogger(int level, const char *msg);
	static int lwsLogLevel(Log::Level lvl);

	/** Parse HTTPd and WebSocket related options */
	int parse(json_t *json);

	Api * getApi()
	{
		return api;
	}

	/* for C-compatability */
	lws_context * getContext()
	{
		return context;
	}

	lws_vhost * getVHost()
	{
		return vhost;
	}

	enum State getState() const
	{
		return state;
	}

	void callbackOnWritable(struct lws *wsi);

	bool isEnabled()
	{
		return port != CONTEXT_PORT_NO_LISTEN;
	}
};

} // namespace node
} // namespace villas
