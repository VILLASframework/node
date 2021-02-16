/** LWS-releated functions.
 *
 * @file
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
 *********************************************************************************/

#pragma once

#include <atomic>
#include <thread>

#include <jansson.h>

#include <villas/log.hpp>
#include <villas/common.hpp>
#include <villas/queue.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Api;

class Web {

protected:
	enum State state;

	Logger logger;

	lws_context *context;		/**< The libwebsockets server context. */
	lws_vhost *vhost;		/**< The libwebsockets vhost. */

	Queue<lws *> writables;		/**< Queue of WSIs for which we will call lws_callback_on_writable() */

	int port;			/**< Port of the build in HTTP / WebSocket server. */
	std::string htdocs;		/**< The root directory for files served via HTTP. */
	std::string ssl_cert;		/**< Path to the SSL certitifcate for HTTPS / WSS. */
	std::string ssl_private_key;	/**< Path to the SSL private key for HTTPS / WSS. */

	std::thread thread;
	std::atomic<bool> running;	/**< Atomic flag for signalizing thread termination. */

	Api *api;

	void worker();
	static void lwsLogger(int level, const char *msg);

public:

	/** Initialize the web interface.
 	 *
 	 * The web interface is based on the libwebsockets library.
 	 */
	Web(Api *a = nullptr);

	void start();
	void stop();

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
};

} /* namespace node */
} /* namespace villas */
