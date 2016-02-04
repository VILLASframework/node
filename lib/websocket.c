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

#ifdef WITH_JANSSON
  #include <jansson.h>
#endif

#include "websocket.h"
#include "timing.h"
#include "utils.h"
#include "msg.h"
#include "cfg.h"

/* Forward declarations */
static struct node_type vt;

static int protocol_cb_http(struct lws *, enum lws_callback_reasons, void *, void *, size_t);
static int protocol_cb_live(struct lws *, enum lws_callback_reasons, void *, void *, size_t);

/* Private static storage */
static config_setting_t *cfg_root;		/**< Root config */
static pthread_t thread;			/**< All nodes are served by a single websocket server. This server is running in a dedicated thread. */
static struct lws_context *context;		/**< The libwebsockets server context. */

static int port;				/**< Port of the build in HTTP / WebSocket server */

static const char *ssl_cert;			/**< Path to the SSL certitifcate for HTTPS / WSS */
static const char *ssl_private_key;		/**< Path to the SSL private key for HTTPS / WSS */
static const char *htdocs;			/**< Path to the directory which should be served by build in HTTP server */

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

static int protocol_cb_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	switch (reason) {
		case LWS_CALLBACK_HTTP:			
			if (!htdocs) {
				lws_return_http_status(wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL);
				goto try_to_reuse;
			}

			if (len < 1) {
				lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
				goto try_to_reuse;
			}

			char *requested_uri = (char *) in;
			
			debug(3, "WebSocket: New HTTP request: %s", requested_uri);

			/* Handle default path */
			if      (!strcmp(requested_uri, "/")) {
				char *response = "HTTP/1.1 302 Found\r\n"
						 "Content-Length: 0\r\n"
						 "Location: /index.html\r\n"
						 "\r\n";
			
				lws_write(wsi, (void *) response, strlen(response), LWS_WRITE_HTTP);
			
				goto try_to_reuse;
			}
#ifdef WITH_JANSSON
			/* Return list of websocket nodes */
			else if (!strcmp(requested_uri, "/nodes.json")) {
				json_t *json_body = json_array();
								
				list_foreach(struct node *n, &vt.instances) {
					struct websocket *w = n->_vd;

					json_t *json_node = json_pack("{ s: s, s: i, s: i, s: i, s: i }",
						"name",		node_name_short(n),
						"connections",	list_length(&w->connections),
						"state",	n->state,
						"vectorize",	n->vectorize,
						"affinity",	n->affinity
					);
					
					/* Add all additional fields of node here.
					 * This can be used for metadata */	
					json_object_update(json_node, config_to_json(n->cfg));
					
					json_array_append_new(json_body, json_node);
				}
				
				char *body = json_dumps(json_body, JSON_INDENT(4));
					
				char *header =  "HTTP/1.1 200 OK\r\n"
						"Connection: close\r\n"
       						"Content-Type: application/json\r\n"
						"\r\n";
				
				lws_write(wsi, (void *) header, strlen(header), LWS_WRITE_HTTP);
				lws_write(wsi, (void *) body,   strlen(body),   LWS_WRITE_HTTP);

				free(body);
				json_decref(json_body);
				
				return -1;
			}
			else if (!strcmp(requested_uri, "/config.json")) {
				char *body = json_dumps(config_to_json(cfg_root), JSON_INDENT(4));
					
				char *header =  "HTTP/1.1 200 OK\r\n"
						"Connection: close\r\n"
       						"Content-Type: application/json\r\n"
						"\r\n";
				
				lws_write(wsi, (void *) header, strlen(header), LWS_WRITE_HTTP);
				lws_write(wsi, (void *) body,   strlen(body),   LWS_WRITE_HTTP);

				free(body);
				
				return -1;
			}
#endif
			else {
				char path[4069];
				snprintf(path, sizeof(path), "%s%s", htdocs, requested_uri);

				/* refuse to serve files we don't understand */
				char *mimetype = get_mimetype(path);
				if (!mimetype) {
					warn("HTTP: Unknown mimetype for %s", path);
					lws_return_http_status(wsi, HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL);
					return -1;
				}

				int n = lws_serve_http_file(wsi, path, mimetype, NULL, 0);
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

static int protocol_cb_live(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct node *n;
	struct websocket *w;

	char *buf, uri[1024];
	char client_name[128], client_ip[128];

	lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi),
		client_name, sizeof(client_name), 
		client_ip, sizeof(client_ip)
	);
	
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
			
			/* Search for node which matches uri */
			list_foreach(n, &vt.instances) {
				if (!strcmp(n->name, uri + 1)) /* we skip leading '/' */
					goto found;
			}

			warn("WebSocket: Closing Connection for non-existent node: %s", uri + 1);

			return -1;

found:			* (void **) user = n;
			w = n->_vd;
			
			list_push(&w->connections, wsi);
				
			info("WebSocket: New Connection for node: %s from %s (%s)", node_name(n), client_name, client_ip);

			return 0;
		
		case LWS_CALLBACK_CLOSED:
			n = * (struct node **) user;
			w = n->_vd;
				
			list_remove(&w->connections, wsi);
			
			info("WebSocket: Connection closed for node: %s", node_name(n));

			return 0;
		
		case LWS_CALLBACK_SERVER_WRITEABLE:
			n = * (struct node **) user;
			if (!n)
				return -1;
			
			w = n->_vd;
			
			if (!w->write.pool)
				return 0; /* no samples available to send */
			
			pthread_mutex_lock(&w->write.mutex);
			
			size_t bytes = 0;
			
			/* Calculate required buffer size */
			for (int i = 0; i < w->write.cnt; i++) {
				struct msg *src = pool_getrel(w->write.pool, i);
				bytes += MSG_LEN(src->values);
			}
			
			/* Allocate buffer */
			buf = malloc(LWS_SEND_BUFFER_PRE_PADDING + bytes);

			/* Fill buffer */
			for (int i = 0; i < w->write.cnt; i++) {
				struct msg *src = pool_getrel(w->write.pool, i);
				struct msg *dst = (struct msg *) (buf + len + LWS_SEND_BUFFER_PRE_PADDING);

				size_t bytes = MSG_LEN(src->values);
				len += bytes;
				
				memcpy(dst, src, bytes);
			}

			pthread_mutex_unlock(&w->write.mutex);
			
			lws_write(wsi, (unsigned char *) (buf + LWS_SEND_BUFFER_PRE_PADDING), len, LWS_WRITE_BINARY);

			return 0;

		case LWS_CALLBACK_RECEIVE:
			n = * (struct node **) user;
			if (!n)
				return -1;

			w = n->_vd;
			
			if (!w->read.cnt)
				return 0;

			pthread_mutex_lock(&w->read.mutex);

			size_t offset = 0;
			for (int i = 0; i < w->read.cnt; i++) {
				struct msg *dst = pool_getrel(w->read.pool, i);
				struct msg *src = (struct msg *) in + offset;

				memcpy(dst, src, MSG_LEN(src->values));

				offset += MSG_LEN(src->values);
				if (offset >= len)
					break;
			}

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
	
	/* Decrease severity for some errors. */
	if (strstr(msg, "Unable to open") == msg)
		level = LLL_WARN;
		
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
	
	while (lws_service(context, 10) >= 0);
	
	debug(3, "WebSocket: shutdown voluntarily");
	
	return NULL;
}

int websocket_init(int argc, char * argv[], config_setting_t *cfg)
{
	config_setting_t *cfg_http;

	lws_set_log_level((1 << LLL_COUNT) - 1, logger);
	
	/* Parse global config */
	cfg_http = config_setting_lookup(cfg, "http");
	if (cfg_http) {
		config_setting_lookup_string(cfg_http, "ssl_cert", &ssl_cert);
		config_setting_lookup_string(cfg_http, "ssl_private_key", &ssl_private_key);
		config_setting_lookup_string(cfg_http, "htdocs", &htdocs);
		config_setting_lookup_int(cfg_http, "port", &port);
	}

	/* Default settings */
	if (!port)
		port = 80;
	if (!htdocs)
		htdocs = "/s2ss/contrib/websocket";
	
	/* Start server */
	struct lws_context_creation_info info = {
		.port = port,
		.protocols = protocols,
		.extensions = lws_get_internal_extensions(),
		.ssl_cert_filepath = ssl_cert,
		.ssl_private_key_filepath = ssl_private_key,
		.gid = -1,
		.uid = -1
	};

	context = lws_create_context(&info);
	if (context == NULL)
		error("WebSocket: failed to initialize server");
	
	/* Save root config for GET /config.json request */
	cfg_root = cfg;
	
	pthread_create(&thread, NULL, server_thread, NULL);

	return 0;
}

int websocket_deinit()
{
	pthread_cancel(thread);
	pthread_join(thread, NULL);

	lws_cancel_service(context);
	lws_context_destroy(context);
	
	return 0;
}

int websocket_open(struct node *n)
{
	struct websocket *w = n->_vd;

	list_init(&w->connections, NULL);
	
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

	pthread_mutex_destroy(&w->read.mutex);
	pthread_mutex_destroy(&w->write.mutex);
	pthread_cond_destroy(&w->read.cond);

	return 0;
}

int websocket_read(struct node *n, struct pool *pool, int cnt)
{
	struct websocket *w = n->_vd;
	
	w->read.pool = pool;
	w->read.cnt = cnt;
	
	pthread_cond_wait(&w->read.cond, &w->read.mutex);
	
	return 1;
}

int websocket_write(struct node *n, struct pool *pool, int cnt)
{
	struct websocket *w = n->_vd;

	pthread_mutex_lock(&w->write.mutex);

	w->write.pool = pool;
	w->write.cnt = cnt;
	
	/* Notify all active websocket connections to send new data */
	list_foreach(struct lws *wsi, &w->connections)
		lws_callback_on_writable(wsi);
	
	pthread_mutex_unlock(&w->write.mutex);
		
	return 1;
}

static struct node_type vt = {
	.name		= "websocket",
	.description	= "Send and receive samples of a WebSocket connection (libwebsockets)",
	.vectorize	= 0, /* unlimited */
	.size		= sizeof(struct websocket),
	.open		= websocket_open,
	.close		= websocket_close,
	.read		= websocket_read,
	.write		= websocket_write,
	.init		= websocket_init,
	.deinit		= websocket_deinit
};

REGISTER_NODE_TYPE(&vt)