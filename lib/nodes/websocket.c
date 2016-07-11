/** Node type: Websockets (libwebsockets)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <libwebsockets.h>
#include <libconfig.h>

#include "nodes/websocket.h"
#include "timing.h"
#include "utils.h"
#include "msg.h"
#include "cfg.h"
#include "config.h"

/* Private static storage */
static config_setting_t *cfg_root;		/**< Root config */
static pthread_t thread;			/**< All nodes are served by a single websocket server. This server is running in a dedicated thread. */
static struct lws_context *context;		/**< The libwebsockets server context. */

static int port;				/**< Port of the build in HTTP / WebSocket server */

static const char *ssl_cert;			/**< Path to the SSL certitifcate for HTTPS / WSS */
static const char *ssl_private_key;		/**< Path to the SSL private key for HTTPS / WSS */
static const char *htdocs;			/**< Path to the directory which should be served by build in HTTP server */

/* Forward declarations */
static struct node_type vt;
static int protocol_cb_http(struct lws *, enum lws_callback_reasons, void *, void *, size_t);
static int protocol_cb_live(struct lws *, enum lws_callback_reasons, void *, void *, size_t);

static struct lws_protocols protocols[] = {
	{
		"http-only",
		protocol_cb_http,
		0,
		0
	},
	{
		"live",
		protocol_cb_live,
		sizeof(struct websocket_connection),
		0
	},
	{ 0  /* terminator */ }
};

#if 0
static const struct lws_extension exts[] = {
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
	{ NULL, NULL, NULL /* terminator */ }
};
#endif

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
		default:    debug(DBG_WEBSOCKET | 1, "LWS: %.*s", len, msg); break;
	}
}

static void * server_thread(void *ctx)
{
	debug(DBG_WEBSOCKET | 3, "WebSocket: Started server thread");
	
	while (lws_service(context, 10) >= 0);
	
	debug(DBG_WEBSOCKET | 3, "WebSocket: shutdown voluntarily");
	
	return NULL;
}

/* Choose mime type based on the file extension */
static char * get_mimetype(const char *resource_path)
{
	char *extension = strrchr(resource_path, '.');

	if (extension == NULL)
		return "text/plain";
	else if (!strcmp(extension, ".png"))
		return "image/png";
	else if (!strcmp(extension, ".svg"))
		return "image/svg+xml";
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

int protocol_cb_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
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
			
			debug(DBG_WEBSOCKET | 3, "LWS: New HTTP request: %s", requested_uri);

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

int protocol_cb_live(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct websocket_connection *c = user;
	struct websocket *w;
	
	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_ESTABLISHED: {
			/* Get path of incoming request */
			char uri[64];
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
			if (strlen(uri) <= 0) {
				warn("LWS: Closing connection with invalid URL: %s", uri);
				return -1;
			}
			
			/* Search for node whose name matches the URI. */
			c->node = list_lookup(&vt.instances, uri + 1);
			if (c->node == NULL) {
				warn("LWS: Closing Connection for non-existent node: %s", uri + 1);
				return -1;
			}
			
			/* Check if node is running */
			if (c->node->state != NODE_RUNNING)
				return -1;
			
			c->state = WEBSOCKET_ESTABLISHED;
			c->wsi = wsi;

			/* Lookup peer address for debug output */
			lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), c->peer.name, sizeof(c->peer.name), c->peer.ip, sizeof(c->peer.ip));

			info("LWS: New Connection for node %s from %s (%s)", node_name(c->node), c->peer.name, c->peer.ip);

			struct websocket *w = (struct websocket *) c->node->_vd;
			list_push(&w->connections, c);

			return 0;
		}

		case LWS_CALLBACK_CLOSED:
			info("LWS: Connection closed for node %s from %s (%s)", node_name(c->node), c->peer.name, c->peer.ip);
			
			c->state = WEBSOCKET_CLOSED;
			c->wsi = NULL;

			return 0;
			
		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			w = (struct websocket *) c->node->_vd;

			if (c->node->state != NODE_RUNNING)
				return -1;

			if (w->shutdown) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY, (unsigned char *) "Bye", 4);
				return -1;
			}
				
			
			int cnt, sent, ret;
			unsigned char *bufs[DEFAULT_QUEUELEN];
			
			cnt = queue_get_many(&w->queue_tx, (void **) bufs, DEFAULT_QUEUELEN, c->sent);

			for (sent = 0; sent < cnt; sent++) {
				struct msg *msg = (struct msg *) (bufs[sent] + LWS_PRE);
				
				ret = lws_write(wsi, (unsigned char *) msg, MSG_LEN(msg->length), LWS_WRITE_BINARY);
				if (ret < MSG_LEN(msg->length))
					error("Failed lws_write()");

				if (lws_send_pipe_choked(wsi))
						break;
			}
			
			queue_pull_many(&w->queue_tx, (void **) bufs, sent, &c->sent);
			
			pool_put_many(&w->pool, (void **) bufs, sent);

			if (sent < cnt)
				lws_callback_on_writable(wsi);

			return 0;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE: {
			w = (struct websocket *) c->node->_vd;

			if (c->node->state != NODE_RUNNING)
				return -1;

			if (!lws_frame_is_binary(wsi) || len < MSG_LEN(0))
				warn("LWS: Received invalid packet for node: %s", node_name(c->node));
			
			struct msg *msg = (struct msg *) in;
			
			while ((char *) msg + MSG_LEN(msg->length) <= (char *) in + len) {
				struct msg *msg2 = pool_get(&w->pool);
				if (!msg2) {
					warn("Pool underrun for node: %s", node_name(c->node));
					return -1;
				}
				
				memcpy(msg2, msg, MSG_LEN(msg->length));
				
				queue_push(&w->queue_rx, msg2, &c->received);
				
				/* Next message */
				msg = (struct msg *) ((char *) msg + MSG_LEN(msg->length));
			}
		
			/** @todo Implement */
			return 0;
		}

		default:
			return 0;
	}
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
		htdocs = "/villas/contrib/websocket";
	
	/* Start server */
	struct lws_context_creation_info info = {
		.port = port,
		.protocols = protocols,
		.extensions = NULL, //exts,
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
	lws_cancel_service(context);
	lws_context_destroy(context);

	pthread_cancel(thread);
	pthread_join(thread, NULL);
	
	return 0;
}

int websocket_open(struct node *n)
{
	struct websocket *w = n->_vd;

	int ret;

	list_init(&w->connections);
	list_init(&w->destinations);
	
	size_t blocklen = LWS_PRE + MSG_LEN(DEFAULT_VALUES);
	
	ret = pool_init_mmap(&w->pool, blocklen, 2 * DEFAULT_QUEUELEN);
	if (ret)
		return ret;
	
	ret = queue_init(&w->queue_tx, DEFAULT_QUEUELEN);
	if (ret)
		return ret;
	
	ret = queue_init(&w->queue_rx, DEFAULT_QUEUELEN);
	if (ret)
		return ret;
	
	queue_reader_add(&w->queue_rx, 0, 0);
	
	return 0;
}

int websocket_close(struct node *n)
{
	struct websocket *w = n->_vd;
	
	w->shutdown = 1;
	
	list_foreach(struct lws *wsi, &w->connections)
		lws_callback_on_writable(wsi);
	
	pool_destroy(&w->pool);
	queue_destroy(&w->queue_tx);
	
	list_destroy(&w->connections, NULL, false);
		
	return 0;
}

int websocket_destroy(struct node *n)
{
//	struct websocket *w = n->_vd;

	return 0;
}

int websocket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct websocket *w = n->_vd;

	struct msg *msgs[cnt];
	
	int got;
	
	got = queue_pull_many(&w->queue_rx, (void **) msgs, cnt, &w->received);
	
	if (got)
	info("got %u", got);
	
	for (int i = 0; i < got; i++) {
		smps[i]->sequence  = msgs[i]->sequence;
		smps[i]->length    = msgs[i]->length;
		smps[i]->ts.origin = MSG_TS(msgs[i]);
		
		memcpy(&smps[i]->data, &msgs[i]->data, MSG_DATA_LEN(msgs[i]->length));
	}
	
	pool_put_many(&w->pool, (void **) msgs, got);

	return got;
}

int websocket_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct websocket *w = n->_vd;

	int blocks, enqueued;
	char *bufs[cnt];

	/* Copy samples to websocket queue */
	blocks = pool_get_many(&w->pool, (void **) bufs, cnt);
	if (blocks != cnt)
		warn("Pool underrun in websocket node: %s", node_name(n));
	
	for (int i = 0; i < blocks; i++) {
		struct msg *msg = (struct msg *) (bufs[i] + LWS_PRE);
		
		msg->version  = MSG_VERSION;
		msg->type     = MSG_TYPE_DATA;
		msg->endian   = MSG_ENDIAN_HOST;
		msg->length   = smps[i]->length;
		msg->sequence = smps[i]->sequence;
		msg->ts.sec   = smps[i]->ts.origin.tv_sec;
		msg->ts.nsec  = smps[i]->ts.origin.tv_nsec;
		
		memcpy(&msg->data, &smps[i]->data, smps[i]->length * 4);
	}
	
	enqueued = queue_push_many(&w->queue_tx, (void **) bufs, cnt, &w->sent);
	if (enqueued != blocks)
		warn("Queue overrun in websocket node: %s", node_name(n));
	
	/* Notify all active websocket connections to send new data */
	list_foreach(struct websocket_connection *c, &w->connections) {
		switch (c->state) {
			case WEBSOCKET_CLOSED:
				queue_reader_remove(&w->queue_tx, c->sent, w->sent);
				list_remove(&w->connections, c);
				break;
			
			case WEBSOCKET_ESTABLISHED:
				c->sent = w->sent;
				c->state = WEBSOCKET_ACTIVE;
				
				queue_reader_add(&w->queue_tx, c->sent, w->sent);
	
			case WEBSOCKET_ACTIVE:
				lws_callback_on_writable(c->wsi);
				break;
		}
	}

	return cnt;
}

static struct node_type vt = {
	.name		= "websocket",
	.description	= "Send and receive samples of a WebSocket connection (libwebsockets)",
	.vectorize	= 0, /* unlimited */
	.size		= sizeof(struct websocket),
	.open		= websocket_open,
	.close		= websocket_close,
	.destroy	= websocket_destroy,
	.read		= websocket_read,
	.write		= websocket_write,
	.init		= websocket_init,
	.deinit		= websocket_deinit
};

REGISTER_NODE_TYPE(&vt)