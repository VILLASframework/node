/** Node type: Websockets (libwebsockets)
 *
 * This file implements the weboscket subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <signal.h>
#include <libwebsockets.h>
#include <libconfig.h>

#include "websocket.h"
#include "timing.h"
#include "utils.h"
#include "msg.h"

/* Forward declarations */
static int protocol_cb_http(struct lws_context *, struct lws *, enum lws_callback_reasons, void *, void *, size_t);
static int protocol_cb_live(struct lws_context *, struct lws *, enum lws_callback_reasons, void *, void *, size_t);

/* Static storage */
static struct websocket_global {
	pthread_t thread;			/**< All nodes are served by a single websocket server. This server is running in a dedicated thread. */
	struct lws_context *context;		/**< The libwebsockets server context. */
	
	struct list nodes;			/**< List of websocket node instances. */

	int port;				/**< Port of the build in HTTP / WebSocket server */

	const char *ssl_cert;			/**< Path to the SSL certitifcate for HTTPS / WSS */
	const char *ssl_private_key;		/**< Path to the SSL private key for HTTPS / WSS */
	const char *htdocs;			/**< Path to the directory which should be served by build in HTTP server */
} global;

static struct lws_protocols protocols[] = {
	{ "http-only",	protocol_cb_http, 0, 0 },
	{ "live",	protocol_cb_live, sizeof(struct node *), 10 },
	{ 0 } /* terminator */
};

/* Choose mime type based on the file extension */
static char * get_mimetype(const char *resource_path)
{
	char *extension = strrchr(resource_path, '.');

	if (extension == NULL)
		return "text/plain";
	else if (!strcmp(extension, ".png"))
		return "image/png";
	else if (!strcmp(extension, ".jpg"))
		return "image/jpg";
	else if (!strcmp(extension, ".gif"))
		return "image/gif";
	else if (!strcmp(extension, ".html"))
		return "text/html";
	else if (!strcmp(extension, ".css"))
		return "text/css";
	else if (!strcmp(extension, ".js"))
		return "application/javascript";
	else
		return "text/plain";
}

static int protocol_cb_http(struct lws_context *context, struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct websocket_global *global = lws_context_user(context);

	switch (reason) {
		case LWS_CALLBACK_HTTP:			
			if (!global->htdocs) {
				lws_return_http_status(context, wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL);
				goto try_to_reuse;
			}

			if (len < 1) {
				lws_return_http_status(context, wsi, HTTP_STATUS_BAD_REQUEST, NULL);
				goto try_to_reuse;
			}

			char *requested_uri = (char *) in;
			
			debug(3, "Got new lws HTTP request: %s", requested_uri);

			/* Handle default path */
			if      (!strcmp(requested_uri, "/")) {
				char *response = "HTTP/1.1 302 Found\r\n"
						 "Content-Length: 0\r\n"
						 "Location: /index.html\r\n"
						 "\r\n";
			
				lws_write(wsi, (void *) response, strlen(response), LWS_WRITE_HTTP);
			
				goto try_to_reuse;
			}
			/* Return list of websocket nodes */
			else if (!strcmp(requested_uri, "/nodes.json")) {
				char response[4096];
				char *body = NULL;
				
				list_foreach(struct node *n, &global->nodes)
					strcatf(&body, "'%s', ", n->name);
				
				snprintf(response, sizeof(response),
					"HTTP/1.1 200 OK\r\n"
					"Connection: close\r\n"
					"\r\n"
					"[ %.*s ]", (int) strlen(body) - 2, body);
				
				lws_write(wsi, (void *) response, strlen(response), LWS_WRITE_HTTP);
				free(body);
				
				return -1;
			}
			else {
				char path[4069];
				snprintf(path, sizeof(path), "%s%s", global->htdocs, requested_uri);

				/* refuse to serve files we don't understand */
				char *mimetype = get_mimetype(path);
				if (!mimetype) {
					warn("HTTP: Unknown mimetype for %s", path);
					lws_return_http_status(context, wsi, HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL);
					return -1;
				}

				int n = lws_serve_http_file(context, wsi, path, mimetype, NULL, 0);
				if      (n < 0)
					return -1;
				else if (n == 0)
					break;
				else
					goto try_to_reuse;
			}

		default:
			break;
	}

	return 0;
	
try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}

static int protocol_cb_live(struct lws_context *context, struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct node *n;
	struct websocket *w;
	struct websocket_global *global = lws_context_user(context);

	char *buf, uri[1024];
	
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
			
			/* Search for node which matches uri */
			list_foreach(n, &global->nodes) {
				if (!strcmp(n->name, uri + 1)) /* we skip leading '/' */
					goto found;
			}

			warn("WebSocket: New Connection for non-exsitent node: %s", uri + 1);

			return -1;

found:			* (void **) user = n;
			w = n->_vd;
			
			list_push(&w->connections, wsi);
				
			info("WebSocket: New Connection for node: %s", n->name);

			return 0;
		
		case LWS_CALLBACK_CLOSED:
			n = * (struct node **) user;
			w = n->_vd;
				
			list_remove(&w->connections, wsi);
			
			info("WebSocket: Connection closed for node: %s", n->name);

			return 0;
		
		case LWS_CALLBACK_SERVER_WRITEABLE:
			n = * (struct node **) user;
			if (!n)
				return 0;
			
			w = n->_vd;
			if (!w->write.m)
				return 0;
			
			buf = malloc(LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING + 4096);
			
			pthread_mutex_lock(&w->write.mutex);

			len = msg_print(buf + LWS_SEND_BUFFER_PRE_PADDING, 4096, w->write.m, MSG_PRINT_NANOSECONDS | MSG_PRINT_VALUES, 0);

			pthread_mutex_unlock(&w->write.mutex);
			
			lws_write(wsi, (unsigned char *) (buf + LWS_SEND_BUFFER_PRE_PADDING), len, LWS_WRITE_TEXT);

			return 0;

		case LWS_CALLBACK_RECEIVE:
			n = * (struct node **) user;
			w = n->_vd;

			pthread_mutex_lock(&w->read.mutex);
			
			msg_scan(in, w->read.m, NULL, NULL);

			pthread_mutex_unlock(&w->read.mutex);			
			pthread_cond_broadcast(&w->read.cond);

			return 0;
			
		default:
			return 0;
	}
}

static void logger(int level, const char *msg) {
	int len = strlen(msg);
	if (strchr(msg, '\n'))
		len -= 1;
		
	switch (level) {
		case LLL_ERR:  error("WebSocket: %.*s", len, msg); break;
		case LLL_WARN:	warn("WebSocket: %.*s", len, msg); break;
		case LLL_INFO:	info("WebSocket: %.*s", len, msg); break;
		default:    debug(1, "WebSocket: %.*s", len, msg); break;
	}
}

static void * server_thread(void *ctx)
{
	debug(3, "WebSocket: Started server thread");
	
	while (lws_service(global.context, 10) >= 0);
	
	return NULL;
}

int websocket_init(int argc, char * argv[], config_setting_t *cfg)
{
	config_setting_t *cfg_http;

	lws_set_log_level((1 << LLL_COUNT) - 1, logger);
	
	/* Parse global config */
	cfg_http = config_lookup_from(cfg, "http");
	if (cfg_http) {
		config_setting_lookup_string(cfg_http, "ssl_cert", &global.ssl_cert);
		config_setting_lookup_string(cfg_http, "ssl_private_key", &global.ssl_private_key);
		config_setting_lookup_string(cfg_http, "htdocs", &global.htdocs);
		config_setting_lookup_int(cfg_http, "port", &global.port);
	}

	/* Default settings */
	if (!global.port)
		global.port = 80;
	if (!global.htdocs)
		global.htdocs = "/s2ss/contrib/websocket";
	
	/* Initialize list of nodes */
	list_init(&global.nodes, NULL);
	
	/* Start server */
	struct lws_context_creation_info info = {
		.port = global.port,
		.protocols = protocols,
		.extensions = lws_get_internal_extensions(),
		.ssl_cert_filepath = global.ssl_cert,
		.ssl_private_key_filepath = global.ssl_private_key,
		.gid = -1,
		.uid = -1,
		.user = &global
	};

	global.context = lws_create_context(&info);
	if (global.context == NULL)
		error("WebSocket: failed to initialize server");
	
	pthread_create(&global.thread, NULL, server_thread, NULL);

	return 0;
}

int websocket_deinit()
{
	pthread_join(global.thread, NULL);

	lws_cancel_service(global.context);
	lws_context_destroy(global.context);
	
	list_destroy(&global.nodes);

	return 0;
}

int websocket_open(struct node *n)
{
	struct websocket *w = n->_vd;

	list_init(&w->connections, NULL);
	list_push(&global.nodes, n);
	
	pthread_mutex_init(&w->read.mutex, NULL);
	pthread_mutex_init(&w->write.mutex, NULL);
	pthread_cond_init(&w->read.cond, NULL);
	
	/* pthread_cond_wait() expects the mutex to be already locked */
	pthread_mutex_lock(&w->read.mutex);
	
	return 0;
}

int websocket_close(struct node *n)
{
	struct websocket *w = n->_vd;
	
	list_remove(&global.nodes, n);

	pthread_mutex_destroy(&w->read.mutex);
	pthread_mutex_destroy(&w->write.mutex);
	pthread_cond_destroy(&w->read.cond);

	return 0;
}

int websocket_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct websocket *w = n->_vd;
	struct msg *m = pool + (first % poolsize);
	
	w->read.m = m;
	
	pthread_cond_wait(&w->read.cond, &w->read.mutex);
	
	return 1;
}

int websocket_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct websocket *w = n->_vd;
	struct msg *m = pool + (first % poolsize);

	pthread_mutex_lock(&w->write.mutex);

	w->write.m = m;
	
	/* Notify all active websocket connections to send new data */
	list_foreach(struct lws *wsi, &w->connections) {
		lws_callback_on_writable(global.context, wsi);
	}
	
	pthread_mutex_unlock(&w->write.mutex);
		
	return 0;
}

static struct node_type vt = {
	.name		= "websocket",
	.description	= "Send and receive samples of a WebSocket connection (libwebsockets)",
	.size		= sizeof(struct websocket),
	.open		= websocket_open,
	.close		= websocket_close,
	.read		= websocket_read,
	.write		= websocket_write,
	.init		= websocket_init,
	.deinit		= websocket_deinit
};

REGISTER_NODE_TYPE(&vt)