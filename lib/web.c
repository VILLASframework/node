/** LWS-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <libconfig.h>
#include <libwebsockets.h>

#include <linux/limits.h>

#include "utils.h"
#include "log.h"
#include "web.h"
#include "api/session.h"

#include "nodes/websocket.h"

/* Forward declarations */
lws_callback_function api_ws_protocol_cb;
lws_callback_function api_http_protocol_cb;
lws_callback_function websocket_protocol_cb;

/** List of libwebsockets protocols. */
static struct lws_protocols protocols[] = {
	{
		.name = "http-api",
		.callback = api_http_protocol_cb,
		.per_session_data_size = sizeof(struct api_session),
		.rx_buffer_size = 0
	},
	{
		.name = "api",
		.callback = api_ws_protocol_cb,
		.per_session_data_size = sizeof(struct api_session),
		.rx_buffer_size = 0
	},
#if 0 /* not supported yet */
	{
		.name = "log",
		.callback = log_ws_protocol_cb,
		.per_session_data_size = 0,
		.rx_buffer_size = 0
	},
	{
		.name = "stats",
		.callback = stats_ws_protocol_cb,
		.per_session_data_size = sizeof(struct api_session),
		.rx_buffer_size = 0
	},
#endif
	{
		.name = "live",
		.callback = websocket_protocol_cb,
		.per_session_data_size = sizeof(struct websocket_connection),
		.rx_buffer_size = 0
	},
	{ NULL /* terminator */ }
};

/** List of libwebsockets mounts. */
static struct lws_http_mount mounts[] = {
	{
		.mount_next = &mounts[1],
		.mountpoint = "/",
		.origin = NULL,
		.def = "/index.html",
		.cgienv = NULL,
		.cgi_timeout = 0,
		.cache_max_age = 0,
		.cache_reusable = 0,
		.cache_revalidate = 0,
		.cache_intermediaries = 0,
		.origin_protocol = LWSMPRO_FILE,
		.mountpoint_len = 1
	},
	{
		.mount_next = NULL,
		.mountpoint = "/api/v1/",
		.origin = "http-api",
		.def = NULL,
		.cgienv = NULL,
		.cgi_timeout = 0,
		.cache_max_age = 0,
		.cache_reusable = 0,
		.cache_revalidate = 0,
		.cache_intermediaries = 0,
		.origin_protocol = LWSMPRO_CALLBACK,
		.mountpoint_len = 8
	}
};

/** List of libwebsockets extensions. */
static const struct lws_extension extensions[] = {
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
	{ NULL /* terminator */ }
};

static void logger(int level, const char *msg) {
	int len = strlen(msg);
	if (strchr(msg, '\n'))
		len -= 1;
	
	/* Decrease severity for some errors. */
	if (strstr(msg, "Unable to open") == msg)
		level = LLL_WARN;
		
	switch (level) {
		case LLL_ERR:   warn("LWS: %.*s", len, msg); break;
		case LLL_WARN:	warn("LWS: %.*s", len, msg); break;
		case LLL_INFO:	info("LWS: %.*s", len, msg); break;
		default:    debug(LOG_WEBSOCKET | 1, "LWS: %.*s", len, msg); break;
	}
}

static void * worker(void *ctx)
{
	struct web *w = ctx;
	
	assert(w->state == STATE_STARTED);

	for (;;)
		lws_service(w->context, 100);
	
	return NULL;
}

int web_init(struct web *w, struct api *a)
{
	lws_set_log_level((1 << LLL_COUNT) - 1, logger);

	w->api = a;
	
	/* Default values */
	w->port = 80;
	w->htdocs = WEB_PATH;
	
	w->state = STATE_INITIALIZED;

	return 0;
}

int web_parse(struct web *w, config_setting_t *cfg)
{
	if (!config_setting_is_group(cfg))
		cerror(cfg, "Setting 'http' must be a group.");

	config_setting_lookup_string(cfg, "ssl_cert", &w->ssl_cert);
	config_setting_lookup_string(cfg, "ssl_private_key", &w->ssl_private_key);
	config_setting_lookup_int(cfg, "port", &w->port);
	config_setting_lookup_string(cfg, "htdocs", &w->htdocs);

	w->state = STATE_PARSED;

	return 0;
}

int web_start(struct web *w)
{
	int ret;

	/* Start server */
	struct lws_context_creation_info ctx_info = {
		.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT,
		.gid = -1,
		.uid = -1,
		.user = (void *) w
	};
	
	struct lws_context_creation_info vhost_info = {
		.protocols = protocols,
		.mounts = mounts,
		.extensions = extensions,
		.port = w->port,
		.ssl_cert_filepath = w->ssl_cert,
		.ssl_private_key_filepath = w->ssl_private_key
	};

	info("Starting Web sub-system: webroot=%s", w->htdocs);

	{ INDENT
		/* update web root of mount point */
		mounts[0].origin = w->htdocs;

		w->context = lws_create_context(&ctx_info);
		if (w->context == NULL)
			error("WebSocket: failed to initialize server");
	
		w->vhost = lws_create_vhost(w->context, &vhost_info);
		if (w->vhost == NULL)
			error("WebSocket: failed to initialize server");
	}
	
	ret = pthread_create(&w->thread, NULL, worker, w);
	if (ret)
		error("Failed to start Web worker");

	w->state = STATE_STARTED;

	return ret;
}

int web_stop(struct web *w)
{
	info("Stopping Web sub-system");

	if (w->state == STATE_STARTED)
		lws_cancel_service(w->context);
	
	/** @todo Wait for all connections to be closed */
	
	pthread_cancel(w->thread);
	pthread_join(w->thread, NULL);
	
	w->state = STATE_STOPPED;

	return 0;
}

int web_destroy(struct web *w)
{
	if (w->state == STATE_DESTROYED)
		return 0;
	
	if (w->context)
		lws_context_destroy(w->context);

	w->state = STATE_DESTROYED;

	return 0;
}
