/** Node type: Websockets (libwebsockets)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <libconfig.h>

#include "super_node.h"
#include "webmsg_format.h"
#include "timing.h"
#include "utils.h"
#include "config.h"
#include "plugin.h"

#include "nodes/websocket.h"

/* Private static storage */
static struct list connections = { .state = STATE_DESTROYED };	/**< List of active libwebsocket connections which receive samples from all nodes (catch all) */

/* Forward declarations */
static struct plugin p;

static char * websocket_connection_name(struct websocket_connection *c)
{
	if (!c->_name) {
		if (c->node)
			asprintf(&c->_name, "%s (%s) for node %s", c->peer.name, c->peer.ip, node_name(c->node));
		else
			asprintf(&c->_name, "%s (%s) for all nodes", c->peer.name, c->peer.ip);
	}
	
	return c->_name;
}

static int websocket_connection_init(struct websocket_connection *c, struct lws *wsi)
{
	int ret;
	
	struct websocket *w = c->node->_vd;
	
	lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), c->peer.name, sizeof(c->peer.name), c->peer.ip, sizeof(c->peer.ip));

	info("LWS: New connection %s", websocket_connection_name(c));

	c->state = STATE_INITIALIZED;
	c->wsi = wsi;
	
	if (c->node != NULL)
		list_push(&w->connections, c);
	else
		list_push(&connections, c);
	
	ret = queue_init(&c->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
	if (ret) {
		warn("Failed to create queue for incoming websocket connection. Closing..");
		return -1;
	}

	return 0;
}

static void websocket_connection_destroy(struct websocket_connection *c)
{
	if (c->state == STATE_DESTROYED)
		return;

	struct websocket *w = c->node->_vd;

	info("LWS: Connection %s closed", websocket_connection_name(c));

	c->state = STATE_DESTROYED;
	c->wsi = NULL;
	
	if (c->node)
		list_remove(&w->connections, c);
	else
		list_remove(&connections, c);

	if (c->_name)
		free(c->_name);
	
	queue_destroy(&c->queue);
}

static void websocket_destination_destroy(struct websocket_destination *d)
{
	free(d->uri);
}

static int websocket_connection_write(struct websocket_connection *c, struct sample *smps[], unsigned cnt)
{
	int blocks, enqueued;
	char *bufs[cnt];
	
	struct websocket *w = c->node->_vd;

	switch (c->state) {
		case STATE_DESTROYED:
		case STATE_STOPPED:
			return -1;

		case STATE_INITIALIZED:
			c->state = STATE_STARTED;
			/* fall through */

		case STATE_STARTED:
			blocks = pool_get_many(&w->pool, (void **) bufs, cnt);
			if (blocks != cnt)
				warn("Pool underrun in websocket connection: %s", websocket_connection_name(c));

			for (int i = 0; i < blocks; i++) {
				struct webmsg *msg = (struct webmsg *) (bufs[i] + LWS_PRE);
	
				msg->version  = WEBMSG_VERSION;
				msg->type     = WEBMSG_TYPE_DATA;
				msg->endian   = WEBMSG_ENDIAN_HOST;
				msg->length   = smps[i]->length;
				msg->sequence = smps[i]->sequence;
				msg->id       = c->node->id;
				msg->ts.sec   = smps[i]->ts.origin.tv_sec;
				msg->ts.nsec  = smps[i]->ts.origin.tv_nsec;
	
				memcpy(&msg->data, &smps[i]->data, smps[i]->length * 4);
			}

			enqueued = queue_push_many(&c->queue, (void **) bufs, cnt);
			if (enqueued != blocks)
				warn("Queue overrun in websocket connection: %s", websocket_connection_name(c));
			
			lws_callback_on_writable(c->wsi);
			break;
			
		default: { }
	}
	
	return 0;
}

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;
	struct websocket_connection *c = user;
	struct websocket *w;
	
	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_ESTABLISHED:
			c->state = STATE_DESTROYED;
			
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
				c->node = list_lookup(&p.node.instances, node);
				if (c->node == NULL) {
					warn("LWS: Closing Connection for non-existent node: %s", uri + 1);
					return -1;
				}
				
				/* Check if node is running */
				if (c->node->state != STATE_STARTED)
					return -1;
			}
			
			ret = websocket_connection_init(c, wsi);
			if (ret)
				return -1;

			return 0;

		case LWS_CALLBACK_CLOSED:
			websocket_connection_destroy(c);
			return 0;
			
		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE:
			w = c->node->_vd;

			if (c->node && c->node->state != STATE_STARTED)
				return -1;

			if (c->state == STATE_STOPPED) {
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

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
			w = c->node->_vd;

			if (c->node->state != STATE_STARTED)
				return -1;

			if (!lws_frame_is_binary(wsi) || len < WEBMSG_LEN(0))
				warn("LWS: Received invalid packet for connection %s", websocket_connection_name(c));
			
			struct webmsg *msg = (struct webmsg *) in;
			
			while ((char *) msg + WEBMSG_LEN(msg->length) <= (char *) in + len) {
				struct webmsg *msg2 = pool_get(&w->pool);
				if (!msg2) {
					warn("Pool underrun for connection %s", websocket_connection_name(c));
					break;
				}
				
				memcpy(msg2, msg, WEBMSG_LEN(msg->length));
				
				ret = queue_push(&w->queue, msg2);
				if (ret != 1) {
					warn("Queue overrun for connection %s", websocket_connection_name(c));
					break;
				}
				
				pthread_mutex_lock(&w->mutex);
				pthread_cond_broadcast(&w->cond);
				pthread_mutex_unlock(&w->mutex);
				
				/* Next message */
				msg = (struct webmsg *) ((char *) msg + WEBMSG_LEN(msg->length));
			}
			return 0;

		default:
			return 0;
	}
}

int websocket_start(struct node *n)
{
	int ret;
	struct websocket *w = n->_vd;
	
	size_t blocklen = LWS_PRE + WEBMSG_LEN(DEFAULT_VALUES);
	
	ret = pool_init(&w->pool, 64 * DEFAULT_QUEUELEN, blocklen, &memtype_hugepage);
	if (ret)
		return ret;
	
	ret = queue_init(&w->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
	if (ret)
		return ret;
	
	ret = pthread_mutex_init(&w->mutex, NULL);
	if (ret)
		return ret;
	
	ret = pthread_cond_init(&w->cond, NULL);
	if (ret)
		return ret;

	/** @todo Connection to destinations via WebSocket client */

	return 0;
}

int websocket_stop(struct node *n)
{
	int ret;
	struct websocket *w = n->_vd;
	
	for (size_t i = 0; i < list_length(&w->connections); i++) {
		struct websocket_connection *c = list_at(&w->connections, i);

		c->state = STATE_STOPPED;

		lws_callback_on_writable(c->wsi);
	}
	
	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	ret = queue_destroy(&w->queue);
	if (ret)
		return ret;
	
	ret = pthread_mutex_destroy(&w->mutex);
	if (ret)
		return ret;
	
	ret = pthread_cond_destroy(&w->cond);
	if (ret)
		return ret;

	return 0;
}

int websocket_destroy(struct node *n)
{
	struct websocket *w = n->_vd;

	list_destroy(&w->connections, NULL, false);
	list_destroy(&w->destinations, (dtor_cb_t) websocket_destination_destroy, true);

	return 0;
}

int websocket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int got;

	struct websocket *w = n->_vd;
	struct webmsg *msgs[cnt];

	do {
		pthread_mutex_lock(&w->mutex);
		pthread_cond_wait(&w->cond, &w->mutex);
		pthread_mutex_unlock(&w->mutex);

		got = queue_pull_many(&w->queue, (void **) msgs, cnt);
		if (got < 0)
			return got;
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

	for (size_t i = 0; i < list_length(&w->connections); i++) {
		struct websocket_connection *c = list_at(&w->connections, i);
	
		websocket_connection_write(c, smps, cnt);
	}
	
	for (size_t i = 0; i < list_length(&connections); i++) {
		struct websocket_connection *c = list_at(&connections, i);
	
		websocket_connection_write(c, smps, cnt);
	}

	return cnt;
}

int websocket_parse(struct node *n, config_setting_t *cfg)
{
	struct websocket *w = n->_vd;
	config_setting_t *cfg_dests;
	int ret;

	list_init(&w->connections);
	list_init(&w->destinations);

	cfg_dests = config_setting_get_member(cfg, "destinations");
	if (cfg_dests) {
		if (!config_setting_is_array(cfg_dests))
			cerror(cfg_dests, "The 'destinations' setting must be an array of URLs");
	
		for (int i = 0; i < config_setting_length(cfg_dests); i++) {
			const char *uri, *prot, *ads, *path;
		
			uri = config_setting_get_string_elem(cfg_dests, i);
			if (!uri)
				cerror(cfg_dests, "The 'destinations' setting must be an array of URLs");
		
			struct websocket_destination d;
		
			d.uri = strdup(uri);
			if (!d.uri)
				serror("Failed to allocate memory");
			
			ret = lws_parse_uri(d.uri, &prot, &ads, &d.info.port, &path);
			if (ret)
				cerror(cfg_dests, "Failed to parse websocket URI: '%s'", uri);
		
			d.info.ssl_connection = !strcmp(prot, "https");
			d.info.address = ads;
			d.info.path = path;
			d.info.protocol = prot;
			d.info.ietf_version_or_minus_one = -1;
		
			list_push(&w->destinations, memdup(&d, sizeof(d)));
		}
	}

	return 0;
}

char * websocket_print(struct node *n)
{
	struct websocket *w = n->_vd;

	char *buf = NULL;
	
	buf = strcatf(&buf, "dests=");
	
	for (size_t i = 0; i < list_length(&w->destinations); i++) {
		struct lws_client_connect_info *in = list_at(&w->destinations, i);

		buf = strcatf(&buf, "%s://%s:%d/%s",
			in->ssl_connection ? "https" : "http",
			in->address,
			in->port,
			in->path
		);
	}
	
	return buf;
}

static struct plugin p = {
	.name		= "websocket",
	.description	= "Send and receive samples of a WebSocket connection (libwebsockets)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct websocket),
		.start		= websocket_start,
		.stop		= websocket_stop,
		.destroy	= websocket_destroy,
		.read		= websocket_read,
		.write		= websocket_write,
		.print		= websocket_print,
		.parse		= websocket_parse,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)