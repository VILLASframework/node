/** Node type: Websockets (libwebsockets)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
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

/* Forward declarations */
static struct node_type vt;

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
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

static struct plugin p = {
	.name		= "websocket",
	.description	= "Send and receive samples of a WebSocket connection (libwebsockets)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct websocket),
		.open		= websocket_open,
		.close		= websocket_close,
		.destroy	= websocket_destroy,
		.read		= websocket_read,
		.write		= websocket_write,
	}
};

REGISTER_PLUGIN(&p)