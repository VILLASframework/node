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

#include <libconfig.h>

#include "nodes/websocket.h"
#include "webmsg_format.h"
#include "timing.h"
#include "utils.h"
#include "msg.h"
#include "cfg.h"
#include "config.h"

/* Internal datastructures */
struct connection {
	enum {
		WEBSOCKET_ESTABLISHED,
		WEBSOCKET_ACTIVE,
		WEBSOCKET_SHUTDOWN,
		WEBSOCKET_CLOSED
	} state;
	
	struct node *node;
	struct path *path;
	
	struct queue queue;			/**< For samples which are sent to the WebSocket */
	
	struct lws *wsi;
	
	struct {
		char name[64];
		char ip[64];
	} peer;
	
	char *_name;
};

struct destination {
	char *uri;
	struct lws_client_connect_info info;
};

/* Private static storage */
static config_setting_t *cfg_root;		/**< Root config */
static pthread_t thread;			/**< All nodes are served by a single websocket server. This server is running in a dedicated thread. */
static struct lws_context *context;		/**< The libwebsockets server context. */

static int port;				/**< Port of the build in HTTP / WebSocket server */

static const char *ssl_cert;			/**< Path to the SSL certitifcate for HTTPS / WSS */
static const char *ssl_private_key;		/**< Path to the SSL private key for HTTPS / WSS */
static const char *htdocs;			/**< Path to the directory which should be served by build in HTTP server */

static int id = 0;

struct list connections;			/**< List of active libwebsocket connections which receive samples from all nodes (catch all) */

/* Forward declarations */
static struct node_type vt;
static int protocol_cb_http(struct lws *, enum lws_callback_reasons, void *, void *, size_t);
static int protocol_cb_live(struct lws *, enum lws_callback_reasons, void *, void *, size_t);

static struct lws_protocols protocols[] = {
	{ "http-only",	protocol_cb_http, 0, 					0 },
	{ "live",	protocol_cb_live, sizeof(struct connection),	0 },
	{ NULL }
};

__attribute__((unused)) static int connection_init(struct connection *c)
{
	/** @todo */
	return -1;
}

__attribute__((unused)) static void connection_destroy(struct connection *c)
{
	if (c->_name)
		free(c->_name);
}

static char * connection_name(struct connection *c)
{
	if (!c->_name) {
		if (c->node)
			asprintf(&c->_name, "%s (%s) for node %s", c->peer.name, c->peer.ip, node_name(c->node));
		else
			asprintf(&c->_name, "%s (%s) for all nodes", c->peer.name, c->peer.ip);
	}
	
	return c->_name;
}

static void destination_destroy(struct destination *d)
{
	free(d->uri);
}

static int connection_write(struct connection *c, struct sample *smps[], unsigned cnt)
{
	int blocks, enqueued;
	char *bufs[cnt];
	
	struct websocket *w = c->node->_vd;

	switch (c->state) {
		case WEBSOCKET_SHUTDOWN:
			return -1;
		case WEBSOCKET_CLOSED:
			if (c->node) {
				struct websocket *w = (struct websocket *) c->node->_vd;
				list_remove(&w->connections, c);
			}
			else
				list_remove(&connections, c);
			break;
		
		case WEBSOCKET_ESTABLISHED:
			c->state = WEBSOCKET_ACTIVE;
			/* fall through */

		case WEBSOCKET_ACTIVE:
			blocks = pool_get_many(&w->pool, (void **) bufs, cnt);
			if (blocks != cnt)
				warn("Pool underrun in websocket connection: %s", connection_name(c));

			for (int i = 0; i < blocks; i++) {
				struct webmsg *msg = (struct webmsg *) (bufs[i] + LWS_PRE);
	
				msg->version  = WEBMSG_VERSION;
				msg->type     = WEBMSG_TYPE_DATA;
				msg->endian   = WEBMSG_ENDIAN_HOST;
				msg->length   = smps[i]->length;
				msg->sequence = smps[i]->sequence;
				msg->id       = w->id;
				msg->ts.sec   = smps[i]->ts.origin.tv_sec;
				msg->ts.nsec  = smps[i]->ts.origin.tv_nsec;
	
				memcpy(&msg->data, &smps[i]->data, smps[i]->length * 4);
			}

			enqueued = queue_push_many(&c->queue, (void **) bufs, cnt);
			if (enqueued != blocks)
				warn("Queue overrun in websocket connection: %s", connection_name(c));
			
			lws_callback_on_writable(c->wsi);
			break;
	}
	
	return 0;
}

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

					json_t *json_node = json_pack("{ s: s, s: i, s: i, s: i, s: i, s: i }",
						"name",		node_name_short(n),
						"id",		w->id,
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
			return 0;
	}

	return 0;
	
try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}

static int protocol_cb_live(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;
	struct connection *c = user;
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
			
			if ((uri[0] == '/' && uri[1] == 0) || uri[0] == 0){
				/* Catch all connection */
				c->node = NULL;
			}
			else {
				char *node = uri + 1;
			
				/* Search for node whose name matches the URI. */
				c->node = list_lookup(&vt.instances, node);
				if (c->node == NULL) {
					warn("LWS: Closing Connection for non-existent node: %s", uri + 1);
					return -1;
				}
				
				/* Check if node is running */
				if (c->node->state != NODE_RUNNING)
					return -1;
			}
			
			c->state = WEBSOCKET_ESTABLISHED;
			c->wsi = wsi;
			
			ret = queue_init(&c->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
			if (ret) {
				warn("Failed to create queue for incoming websocket connection. Closing..");
				return -1;
			}

			/* Lookup peer address for debug output */
			lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), c->peer.name, sizeof(c->peer.name), c->peer.ip, sizeof(c->peer.ip));

			info("LWS: New connection %s", connection_name(c));

			if (c->node != NULL) {
				struct websocket *w = (struct websocket *) c->node->_vd;
				list_push(&w->connections, c);
			}
			else {
				list_push(&connections, c);
			}

			return 0;
		}

		case LWS_CALLBACK_CLOSED:
			info("LWS: Connection %s closed", connection_name(c));
			
			c->state = WEBSOCKET_CLOSED;
			c->wsi = NULL;
			
			queue_destroy(&c->queue);

			return 0;
			
		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			w = (struct websocket *) c->node->_vd;

			if (c->node && c->node->state != NODE_RUNNING)
				return -1;

			if (c->state == WEBSOCKET_SHUTDOWN) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY, (unsigned char *) "Node stopped", 4);
				return -1;
			}

			char *buf;
			int cnt;
			while ((cnt = queue_pull(&c->queue, (void **) &buf))) {
				struct webmsg *msg = (struct webmsg *) (buf + LWS_PRE);
				
				pool_put(&w->pool, (void *) buf);
				
				ret = lws_write(wsi, (unsigned char *) msg, WEBMSG_LEN(msg->length), LWS_WRITE_BINARY);
				if (ret < WEBMSG_LEN(msg->length))
					error("Failed lws_write()");

				if (lws_send_pipe_choked(wsi))
					break;
			}

			if (queue_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);

			return 0;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE: {
			w = (struct websocket *) c->node->_vd;

			if (c->node->state != NODE_RUNNING)
				return -1;

			if (!lws_frame_is_binary(wsi) || len < WEBMSG_LEN(0))
				warn("LWS: Received invalid packet for connection %s", connection_name(c));
			
			struct webmsg *msg = (struct webmsg *) in;
			
			while ((char *) msg + WEBMSG_LEN(msg->length) <= (char *) in + len) {
				struct webmsg *msg2 = pool_get(&w->pool);
				if (!msg2) {
					warn("Pool underrun for connection %s", connection_name(c));
					break;
				}
				
				memcpy(msg2, msg, WEBMSG_LEN(msg->length));
				
				ret = queue_push(&w->queue, msg2);
				if (ret != 1) {
					warn("Queue overrun for connection %s", connection_name(c));
					break;
				}
				
				/* Next message */
				msg = (struct webmsg *) ((char *) msg + WEBMSG_LEN(msg->length));
			}
		
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
	int ret;
	struct websocket *w = n->_vd;
	
	w->id = id++;

	list_init(&w->connections);
	list_init(&w->destinations);
	
	size_t blocklen = LWS_PRE + WEBMSG_LEN(DEFAULT_VALUES);
	
	ret = pool_init(&w->pool, 64 * DEFAULT_QUEUELEN, blocklen, &memtype_hugepage);
	if (ret)
		return ret;
	
	ret = queue_init(&w->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
	if (ret)
		return ret;

	return 0;
}

int websocket_close(struct node *n)
{
	struct websocket *w = n->_vd;
	
	list_foreach(struct connection *c, &w->connections) {
		c->state = WEBSOCKET_SHUTDOWN;
		lws_callback_on_writable(c->wsi);
	}
	
	pool_destroy(&w->pool);
	queue_destroy(&w->queue);
	
	list_destroy(&w->connections, NULL, false);
		
	return 0;
}

int websocket_destroy(struct node *n)
{
	struct websocket *w = n->_vd;
	
	list_destroy(&w->destinations, (dtor_cb_t) destination_destroy, true);

	return 0;
}

int websocket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct websocket *w = n->_vd;

	struct webmsg *msgs[cnt];
	
	int got;
	
	do {
		got = queue_pull_many(&w->queue, (void **) msgs, cnt);
		pthread_yield();
	} while (got == 0);
	
	for (int i = 0; i < got; i++) {
		smps[i]->sequence  = msgs[i]->sequence;
		smps[i]->length    = msgs[i]->length;
		smps[i]->ts.origin = WEBMSG_TS(msgs[i]);
		
		memcpy(&smps[i]->data, &msgs[i]->data, WEBMSG_DATA_LEN(msgs[i]->length));
	}
	
	pool_put_many(&w->pool, (void **) msgs, got);

	return got;
}

int websocket_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct websocket *w = n->_vd;

	list_foreach(struct connection *c, &w->connections) {
		connection_write(c, smps, cnt);
	}
	
	list_foreach(struct connection *c, &connections) {
		connection_write(c, smps, cnt);
	}

	return cnt;
}

int websocket_parse(struct node *n, config_setting_t *cfg)
{
	struct websocket *w = n->_vd;
	config_setting_t *cfg_dests;
	int ret;
	
	cfg_dests = config_setting_get_member(cfg, "destinations");
	if (cfg_dests) {
		if (!config_setting_is_array(cfg_dests))
			cerror(cfg_dests, "The 'destinations' setting must be an array of URLs");
	
		for (int i = 0; i < config_setting_length(cfg_dests); i++) {
			struct destination *d;
			const char *uri, *prot, *ads, *path;
		
			uri = config_setting_get_string_elem(cfg_dests, i);
			if (!uri)
				cerror(cfg_dests, "The 'destinations' setting must be an array of URLs");
		
			d = alloc(sizeof(struct destination));
		
			d->uri = strdup(uri);
			if (!d->uri)
				serror("Failed to allocate memory");
			
			ret = lws_parse_uri(d->uri, &prot, &ads, &d->info.port, &path);
			if (ret)
				cerror(cfg_dests, "Failed to parse websocket URI: '%s'", uri);
		
			d->info.ssl_connection = !strcmp(prot, "https");
			d->info.address = ads;
			d->info.path = path;
			d->info.protocol = prot;
			d->info.ietf_version_or_minus_one = -1;
		
			list_push(&w->destinations, d);
		}
	}

	return 0;
}

char * websocket_print(struct node *n)
{
	struct websocket *w = n->_vd;

	char *buf = NULL;
	
	list_foreach(struct lws_client_connect_info *in, &w->destinations) {
		buf = strcatf(&buf, "%s://%s:%d/%s",
			in->ssl_connection ? "https" : "http",
			in->address,
			in->port,
			in->path
		);
	}
	
	return buf;
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
	.deinit		= websocket_deinit,
	.print		= websocket_print,
	.parse		= websocket_parse
};

REGISTER_NODE_TYPE(&vt)