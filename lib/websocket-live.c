/** Live protocol of the websocket node type
 *
 * This protocol callback function is used to handle the binary websocket protoocol
 * which is used to send / receive struct msg's.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

static int protocol_cb_live(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct conn *c = user;
	
	switch (reason) {
		case LWS_CALLBACK_WSI_CREATE:
			c->wsi = wsi;
			return 0;

		case LWS_CALLBACK_WSI_DESTROY:
			c->wsi = NULL;

			connection_destroy(c); /* release c->peer.ip & c->peer.name */
			return 0;
		
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_ESTABLISHED:
			c->state = CONN_STATE_ESTABLISHED;
			c->role = (reason == LWS_CALLBACK_ESTABLISHED)
				? CONN_ROLE_SERVER
				: CONN_ROLE_CLIENT;

			/* Get path of incoming request */
			char uri[64]
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
			if (strlen(uri) <= 0) {
				warn("WebSocket: Closing connection with invalid URL: %s")
				return -1;
			}
			
			/* Search for node whose name matches the URI. */
			c->node = list_lookup(&vt.instances, uri + 1);
			if (c->node == NULL) {
				warn("WebSocket: Closing Connection for non-existent node: %s", uri + 1);
				return -1;
			}
			
			/* Check if node is running */
			if (c->node.state != NODE_RUNNING)
				return -1;

			/* Alias to ease readability */
			c->ws = n->_vd;

			/* Lookup peer address for debug output */
			c->peer.name = alloc(64);
			c->peer.ip = alloc(64);
			lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), c->peer.name, 64, c->peer.ip, 64);

			info("WebSocket: New Connection for node %s from %s (%s)", node_name(c->node), c->peer.name, c->peer.ip);
			list_push(&c->ws->connections, c);

			return 0;
		
		case LWS_CALLBACK_CLOSED:			
			c->state = CONN_STATE_CLOSED;
			
			info("WebSocket: Connection closed for node %s from %s (%s)", node_name(c->node), c->peer.name, c->peer.ip);
			list_remove(&c->ws->connections, c);

			return 0;
			
		case LWS_CALLBACK_CLIENT_WRITABLE:		
		case LWS_CALLBACK_SERVER_WRITEABLE:
			if (c->node.state != NODE_RUNNING)
				return -1;
			
			int sent, sz, remain;
			struct queue *q = &c->tx_queue;
		
			pthread_mutex_lock(&q->lock);
			
			sz = q->tail - q->head;
			if (len == 0)
				goto out; /* nothing to sent at the moment */
					
			sent = lws_write(wsi, q->head, sz, LWS_WRITE_BINARY);
			if (sent < 0)
				goto out;
				
			/* Move unsent part to head of queue */
			remain = sz - sent;
			if (remain > 0)
				memmove(q->head, q->head + sent, remain);
			
			/* Update queue tail */
			q->tail = q->head + remain;
			
out:			pthread_mutex_unlock(&q->lock);

			return (sent < 0) ? -1 : 0;

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
			if (c->node.state != NODE_RUNNING)
				return -1;
		
			if (!lws_frame_is_binary(wsi)) {
				warn("WebSocket: Received non-binary frame for node %s", node_name(c->node));
				return -1;
			}
			
			int ret = 0;
			struct queue *q = &w->rx_queue;

			pthread_mutex_lock(&q->lock);
			
	
			
			memcpy(q->tail, in, len);
			
			q->tail += len;

			pthread_cond_broadcast(&q->cond);
out2:			pthread_mutex_unlock(&q->lock);			

			return ret;
			
		default:
			return 0;
	}
}