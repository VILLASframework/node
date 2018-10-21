/** LWS-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <libwebsockets.h>
#include <string.h>
#include <pthread.h>

#include <villas/node/config.h>
#include <villas/utils.h>
#include <villas/web.hpp>
#include <villas/api.hpp>
#include <villas/exceptions.hpp>
#include <villas/api/sessions/http.hpp>
#include <villas/api/sessions/websocket.hpp>

#include <villas/nodes/websocket.h>

using namespace villas;
using namespace villas::node;

/* Forward declarations */
lws_callback_function websocket_protocol_cb;

Logger Web::logger = logging.get("web");

/** List of libwebsockets protocols. */
lws_protocols protocols[] = {
 	{
 		.name = "http",
 		.callback = lws_callback_http_dummy,
 		.per_session_data_size = 0,
 		.rx_buffer_size = 1024
 	},
#ifdef WITH_API
	{
		.name = "http-api",
		.callback = api_http_protocol_cb,
		.per_session_data_size = sizeof(api::sessions::Http),
		.rx_buffer_size = 1024
	},
	{
		.name = "api",
		.callback = api_ws_protocol_cb,
		.per_session_data_size = sizeof(api::sessions::WebSocket),
		.rx_buffer_size = 0
	},
#endif /* WITH_API */
#ifdef __LIBWEBSOCKETS_FOUND /** @todo: Port to C++ */
	{
		.name = "live",
		.callback = websocket_protocol_cb,
		.per_session_data_size = sizeof(websocket_connection),
		.rx_buffer_size = 0
	},
#endif /* LIBWEBSOCKETS_FOUND */
	{ nullptr /* terminator */ }
};

/** List of libwebsockets mounts. */
static lws_http_mount mounts[] = {
#ifdef WITH_API
	{
		.mount_next = &mounts[1],
		.mountpoint = "/api/v1",
		.origin = "http-api",
		.def = nullptr,
		.cgienv = nullptr,
		.cgi_timeout = 0,
		.cache_max_age = 0,
		.cache_reusable = 0,
		.cache_revalidate = 0,
		.cache_intermediaries = 0,
		.origin_protocol = LWSMPRO_CALLBACK,
		.mountpoint_len = 7
	},
#endif /* WITH_API */
	{
		.mount_next = nullptr,
		.mountpoint = "/",
		.origin = nullptr,
		.def = "/index.html",
		.cgienv = nullptr,
		.cgi_timeout = 0,
		.cache_max_age = 0,
		.cache_reusable = 0,
		.cache_revalidate = 0,
		.cache_intermediaries = 0,
		.origin_protocol = LWSMPRO_FILE,
		.mountpoint_len = 1
	}
};

/** List of libwebsockets extensions. */
static const lws_extension extensions[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ nullptr /* terminator */ }
};

void Web::lwsLogger(int lws_lvl, const char *msg) {
	char *nl;

	nl = (char *) strchr(msg, '\n');
	if (nl)
		*nl = 0;

	/* Decrease severity for some errors. */
	if (strstr(msg, "Unable to open") == msg)
		lws_lvl = LLL_WARN;

	Logger logger = logging.get("lws");

	switch (lws_lvl) {
		case LLL_ERR:
			logger->error("{}", msg);
			break;

		case LLL_WARN:
			logger->warn("{}", msg);
			break;

		case LLL_NOTICE:
		case LLL_INFO:
			logger->info("{}", msg);
			break;

		default: /* Everything else is debug */
			logger->debug("{}", msg);
	}
}

void Web::worker()
{
	lws *wsi;

	while (running) {
		lws_service(context, 100);

		while (!writables.empty()) {
			wsi = writables.pull();

			lws_callback_on_writable(wsi);
		}
	}

	logger->info("Stopping worker");
}

Web::Web(Api *a) :
	state(STATE_INITIALIZED),
	htdocs(WEB_PATH),
	api(a)
{
	int lvl = LLL_ERR | LLL_WARN | LLL_NOTICE;

	/** @todo: Port to C++: add LLL_DEBUG and others if trace log level is activated */

	lws_set_log_level(lvl, lwsLogger);

	/* Default values */
	port = getuid() > 0 ? 8080 : 80;
}

int Web::parse(json_t *cfg)
{
	int ret, enabled = 1;
	const char *cert = nullptr;
	const char *pkey = nullptr;
	const char *htd = nullptr;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s?: i, s?: b }",
		"ssl_cert", &cert,
		"ssl_private_key", &pkey,
		"htdocs", &htd,
		"port", &port,
		"enabled", &enabled
	);
	if (ret)
		throw new JsonError(err, "Failed to http section of configuration file");

	if (cert)
		ssl_cert = cert;

	if (pkey)
		ssl_private_key = pkey;

	if (htd)
		htdocs = htd;

	if (!enabled)
		port = CONTEXT_PORT_NO_LISTEN;

	state = STATE_PARSED;

	return 0;
}

void Web::start()
{
	/* Start server */
	lws_context_creation_info ctx_info = {
		.port = port,
		.protocols = protocols,
		.extensions = extensions,
		.ssl_cert_filepath = ssl_cert.empty() ? nullptr : ssl_cert.c_str(),
		.ssl_private_key_filepath = ssl_private_key.empty() ? nullptr : ssl_private_key.c_str(),
		.gid = -1,
		.uid = -1,
		.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT,
		.user = (void *) this,
		.mounts = mounts,
	};

	logger->info("Starting sub-system: htdocs={}", htdocs.c_str());
	/* update web root of mount point */
	mounts[1].origin = htdocs.c_str();

	context = lws_create_context(&ctx_info);
	if (context == nullptr)
		throw new RuntimeError("Failed to initialize server context");

	for (int tries = 10; tries > 0; tries--) {
		vhost = lws_create_vhost(context, &ctx_info);
		if (vhost)
			break;

		ctx_info.port++;
		logger->warn("WebSocket: failed to setup vhost. Trying another port: {}", ctx_info.port);
	}

	if (vhost == NULL)
		throw new RuntimeError("Failed to initialize virtual host");

	/* Start thread */
	running = true;
	thread = std::thread(&Web::worker, this);

	state = STATE_STARTED;
}

void Web::stop()
{
	assert(state == STATE_STARTED);

	logger->info("Stopping sub-system");

	running = false;
	thread.join();

	lws_context_destroy(context);

	state = STATE_STOPPED;
}

void Web::callbackOnWritable(lws *wsi)
{
	writables.push(wsi);
}
