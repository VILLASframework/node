/** LWS-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <libconfig.h>
#include <libwebsockets.h>

#include <linux/limits.h>

#if 0
  #include "nodes/websocket.h"
#endif

#include "utils.h"
#include "log.h"
#include "web.h"
#include "api.h"

/* Forward declarations */
lws_callback_function api_protocol_cb;

/** Path to the directory which should be served by build in HTTP server */
static char htdocs[PATH_MAX] = "/usr/local/share/villas/node/htdocs";

/** List of libwebsockets protocols. */
static struct lws_protocols protocols[] = {
	{
		.name = "http-only",
		.callback = api_protocol_cb,
		.per_session_data_size = sizeof(struct api_session),
		.rx_buffer_size = 0
	},
#if 0
	{
		.name = "live",
		.callback = websocket_protocol_cb,
		.per_session_data_size = sizeof(struct websocket_connection),
		.rx_buffer_size = 0
	},
#endif
	{
		.name = "api",
		.callback = api_protocol_cb,
		.per_session_data_size = sizeof(struct api_session),
		.rx_buffer_size = 0
	},
	{ 0  /* terminator */ }
};

/** List of libwebsockets mounts. */
static struct lws_http_mount mounts[] = {
	{
		.mount_next = &mounts[1],
		.mountpoint = "/api/v1/",
		.origin = "cmd",
		.def = NULL,
		.cgienv = NULL,
		.cgi_timeout = 0,
		.cache_max_age = 0,
		.cache_reusable = 0,
		.cache_revalidate = 0,
		.cache_intermediaries = 0,
		.origin_protocol = LWSMPRO_CALLBACK,
		.mountpoint_len = 8
	},
	{
		.mount_next = NULL,
		.mountpoint = "/",
		.origin = htdocs,
		.def = "/index.html",
		.cgienv = NULL,
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
		case LLL_ERR:  error("LWS: %.*s", len, msg); break;
		case LLL_WARN:	warn("LWS: %.*s", len, msg); break;
		case LLL_INFO:	info("LWS: %.*s", len, msg); break;
		default:    debug(LOG_WEBSOCKET | 1, "LWS: %.*s", len, msg); break;
	}
}

int web_service(struct web *w)
{
	return lws_service(w->context, 10);
}

int web_parse(struct web *w, config_setting_t *lcs)
{
	config_setting_t *lcs_http;

	/* Parse global config */
	lcs_http = config_setting_lookup(lcs, "http");
	if (lcs_http) {
		const char *ht;

		config_setting_lookup_string(lcs_http, "ssl_cert", &w->ssl_cert);
		config_setting_lookup_string(lcs_http, "ssl_private_key", &w->ssl_private_key);
		config_setting_lookup_int(lcs_http, "port", &w->port);
		
		if (config_setting_lookup_string(lcs_http, "htdocs", &w->htdocs)) {
			strncpy(htdocs, ht, sizeof(htdocs));
		}
	}
	
	return 0;
}

int web_init(struct web *w, struct api *a)
{
	w->api = a;

	lws_set_log_level((1 << LLL_COUNT) - 1, logger);

	/* Start server */
	struct lws_context_creation_info ctx_info = {
		.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS,
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

	w->context = lws_create_context(&ctx_info);
	if (w->context == NULL)
		error("WebSocket: failed to initialize server");
	
	w->vhost = lws_create_vhost(w->context, &vhost_info);
	if (w->vhost == NULL)
		error("WebSocket: failed to initialize server");

	return 0;
}

int web_destroy(struct web *w)
{
	lws_context_destroy(w->context);

	return 0;
}

int web_deinit(struct web *w)
{
	lws_cancel_service(w->context);

	return 0;
}
