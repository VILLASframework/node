/** Node type: Websockets (libwebsockets)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "super_node.h"
#include "timing.h"
#include "utils.h"
#include "buffer.h"
#include "plugin.h"
#include "io_format.h"
#include "nodes/websocket.h"

/* Private static storage */
static struct list connections = { .state = STATE_DESTROYED };	/**< List of active libwebsocket connections which receive samples from all nodes (catch all) */
static struct web *web;

/* Forward declarations */
static struct plugin p;

static char * websocket_connection_name(struct websocket_connection *c)
{
	if (!c->_name) {
		if (c->wsi) {
			char name[128];
			char ip[128];

			lws_get_peer_addresses(c->wsi, lws_get_socket_fd(c->wsi), name, sizeof(name), ip, sizeof(ip));

			strcatf(&c->_name, "remote.ip=%s, remote.name=%s", ip, name);
		}
		else if (c->destination)
			strcatf(&c->_name, "%s:%d", c->destination->info.address, c->destination->info.port);

		if (c->node)
			strcatf(&c->_name, ", node=%s", node_name(c->node));

		strcatf(&c->_name, ", mode=%s", c->mode == WEBSOCKET_MODE_CLIENT ? "client" : "server");
	}

	return c->_name;
}

static void websocket_destination_destroy(struct websocket_destination *d)
{
	free(d->uri);

	free((char *) d->info.path);
	free((char *) d->info.address);
}

static int websocket_connection_write(struct websocket_connection *c, struct sample *smps[], unsigned cnt)
{
	int pushed;

	pushed = queue_push_many(&c->queue, (void **) smps, cnt);
	if (pushed < cnt)
		warn("Queue overrun in WebSocket connection: %s", websocket_connection_name(c));

	sample_get_many(smps, cnt);

	debug(LOG_WEBSOCKET | 10, "Enqueued %u samples to %s", pushed, websocket_connection_name(c));

	/* Client connections which are currently conecting don't have an associate c->wsi yet */
	if (c->wsi)
		lws_callback_on_writable(c->wsi);

	return 0;
}

static void websocket_connection_close(struct websocket_connection *c, struct lws *wsi, enum lws_close_status status, const char *reason)
{
	lws_close_reason(wsi, status, (unsigned char *) reason, strlen(reason));

	debug(LOG_WEBSOCKET | 10, "Closing WebSocket connection with %s: status=%u, reason=%s", websocket_connection_name(c), status, reason);
}

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret, recvd, pulled, cnt = 128;
	struct websocket_connection *c = user;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			c->wsi = wsi;
			c->state = STATE_ESTABLISHED;

			buffer_init(&c->buffers.recv, 1 << 12);
			buffer_init(&c->buffers.send, 1 << 12);

			debug(LOG_WEBSOCKET | 10, "Established WebSocket connection: %s", websocket_connection_name(c));

			/* Schedule writable callback in case we have something to send */
			if (queue_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);

			break;

		case LWS_CALLBACK_ESTABLISHED:
			c->wsi = wsi;
			c->state = STATE_ESTABLISHED;
			c->mode = WEBSOCKET_MODE_SERVER;
			c->_name = NULL;

			/* We use the URI to associate this connection to a node
			 * and choose a protocol.
			 *
			 * Example: ws://example.com/node_1.json
			 *   Will select the node with the name 'node_1'
			 *   and format 'json'.
			 *
			 * If the node name is omitted, this connection
			 * will receive sample data from all websocket nodes
			 * (catch all).
			 */

			/* Get path of incoming request */
			char *node, *format = NULL;
			char uri[64];

			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
			if (strlen(uri) <= 0) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Invalid URL");
				return -1;
			}

			node = strtok(uri, "/.");
			if (strlen(node) == 0)
				c->node = NULL;
			else {
				format = strtok(NULL, "");

				/* Search for node whose name matches the URI. */
				c->node = list_lookup(&p.node.instances, node);
				if (c->node == NULL) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown node");
					return -1;
				}
			}

			if (!format)
				format = "villas-web";

			c->format = io_format_lookup(format);
			if (!c->format) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Invalid format");
				return -1;
			}

			buffer_init(&c->buffers.recv, 1 << 12);
			buffer_init(&c->buffers.send, 1 << 12);

			ret = queue_init(&c->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
			if (ret)
				return -1;

			list_push(&connections, c);

			debug(LOG_WEBSOCKET | 10, "Established WebSocket connection: %s", websocket_connection_name(c));

			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			c->state = STATE_ERROR;

			warn("Failed to establish WebSocket connection: %s, reason=%s", websocket_connection_name(c), in ? (char *) in : "unkown");

			return -1;

		case LWS_CALLBACK_CLOSED:
			debug(LOG_WEBSOCKET | 10, "Closed WebSocket connection: %s", websocket_connection_name(c));

			if (c->state != STATE_SHUTDOWN) {
				/** @todo Attempt reconnect here */
			}

			list_remove(&connections, c);

			if (c->_name)
				free(c->_name);

			ret = queue_destroy(&c->queue);
			if (ret)
				return ret;

			buffer_destroy(&c->buffers.recv);
			buffer_destroy(&c->buffers.send);

			c->wsi = NULL;

			if (c->mode == WEBSOCKET_MODE_CLIENT)
				free(c);

			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			size_t wbytes;

			if (c->state == STATE_SHUTDOWN) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_GOINGAWAY, "Node stopped");
				return -1;
			}

			struct sample **smps = alloca(cnt * sizeof(struct sample *));

			pulled = queue_pull_many(&c->queue, (void **) smps, cnt);
			if (pulled > 0) {
				io_format_sprint(c->format, c->buffers.send.buf + LWS_PRE, c->buffers.send.size - LWS_PRE, &wbytes, smps, pulled, SAMPLE_ALL);

				ret = lws_write(wsi, (unsigned char *) c->buffers.send.buf + LWS_PRE, wbytes, c->format->flags & IO_FORMAT_BINARY ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);

				sample_put_many(smps, pulled);

				debug(LOG_WEBSOCKET | 10, "Send %d samples to connection: %s, bytes=%d", pulled, websocket_connection_name(c), ret);
			}

			if (queue_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);

			break;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
			if (!c->node) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Catch-all connection can not receive.");
				return -1;
			}

			if (lws_is_first_fragment(wsi))
				buffer_clear(&c->buffers.recv);

			ret = buffer_append(&c->buffers.recv, in, len);
			if (ret) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE, "Failed to process data");
				return -1;
			}

			/* We dont try to parse the frame yet, as we have to wait for the remaining fragments */
			if (lws_is_final_fragment(wsi)) {
				struct timespec ts_recv = time_now();

				struct websocket *w = c->node->_vd;
				struct sample **smps = alloca(cnt * sizeof(struct sample *));

				ret = sample_alloc(&w->pool, smps, cnt);
				if (ret != cnt) {
					warn("Pool underrun for connection: %s", websocket_connection_name(c));
					break;
				}

				recvd = io_format_sscan(c->format, c->buffers.recv.buf, c->buffers.recv.len, NULL, smps, cnt, 0);
				if (recvd < 0) {
					warn("Failed to parse sample data received on connection: %s", websocket_connection_name(c));
					break;
				}

				debug(LOG_WEBSOCKET | 10, "Received %d samples to connection: %s", recvd, websocket_connection_name(c));

				/* Set receive timestamp */
				for (int i = 0; i < recvd; i++) {
					smps[i]->ts.received = ts_recv;
					smps[i]->has |= SAMPLE_RECEIVED;
				}

				ret = queue_signalled_push_many(&w->queue, (void **) smps, recvd);
				if (ret != recvd)
					warn("Queue overrun for connection: %s", websocket_connection_name(c));

				if (c->state == STATE_SHUTDOWN) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_GOINGAWAY, "Node stopped");
					return -1;
				}
			}

			break;

		default:
			break;
	}

	return 0;
}

int websocket_init(struct super_node *sn)
{
	list_init(&connections);

	web = &sn->web;

	if (web->state != STATE_STARTED)
		return -1;

	return 0;
}

int websocket_deinit()
{
	for (size_t i = 0; i < list_length(&connections); i++) {
		struct websocket_connection *c = list_at(&connections, i);

		c->state = STATE_SHUTDOWN;

		lws_callback_on_writable(c->wsi);
	}

	/* Wait for all connections to be closed */
	while (list_length(&connections) > 0) {
		info("Waiting for WebSocket connection shutdown");
		sleep(1);
	}

	list_destroy(&connections, (dtor_cb_t) websocket_destination_destroy, true);

	return 0;
}

int websocket_start(struct node *n)
{
	int ret;
	struct websocket *w = n->_vd;

	ret = pool_init(&w->pool, DEFAULT_WEBSOCKET_QUEUELEN, SAMPLE_LEN(DEFAULT_WEBSOCKET_SAMPLELEN), &memtype_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_WEBSOCKET_QUEUELEN, &memtype_hugepage, 0);
	if (ret)
		return ret;

	for (int i = 0; i < list_length(&w->destinations); i++) {
		struct websocket_destination *d = list_at(&w->destinations, i);
		struct websocket_connection *c = alloc(sizeof(struct websocket_connection));

		c->state = STATE_CONNECTING;
		c->mode = WEBSOCKET_MODE_CLIENT;
		c->node = n;
		c->destination = d;
		c->_name = NULL;

		c->format = io_format_lookup("villas-web"); /** @todo We could parse the format from the URI */

		d->info.context = web->context;
		d->info.vhost = web->vhost;
		d->info.userdata = c;

		ret = queue_init(&c->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
		if (ret)
			return -1;

		list_push(&connections, c);

		lws_client_connect_via_info(&d->info);
	}

	return 0;
}

int websocket_stop(struct node *n)
{
	int ret;
	struct websocket *w = n->_vd;

	/* Wait for all connections to be closed */
	for (;;) {
		int connecting = 0;

		for (int i = 0; i < list_length(&w->destinations); i++) {
			struct websocket_destination *d = list_at(&w->destinations, i);
			struct websocket_connection *c = d->info.userdata;

			if (c->state == STATE_CONNECTING)
				connecting++;
		}

		if (connecting == 0)
			break;

		debug(LOG_WEBSOCKET | 10, "Waiting for %d client connections to be established", connecting);
		sleep(1);
	}

	for (size_t i = 0; i < list_length(&connections); i++) {
		struct websocket_connection *c = list_at(&connections, i);

		if (c->node != n)
			continue;

		if (c->state != STATE_CONNECTING)
			c->state = STATE_SHUTDOWN;

		lws_callback_on_writable(c->wsi);
	}

	ret = queue_signalled_destroy(&w->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	return 0;
}

int websocket_destroy(struct node *n)
{
	struct websocket *w = n->_vd;

	list_destroy(&w->destinations, (dtor_cb_t) websocket_destination_destroy, true);

	return 0;
}

int websocket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int avail;

	struct websocket *w = n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
	if (avail < 0)
		return avail;

	for (int i = 0; i < avail; i++) {
		sample_copy(smps[i], cpys[i]);
		sample_put(cpys[i]);
	}

	return avail;
}

int websocket_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int avail;

	struct websocket *w = n->_vd;
	struct sample *cpys[cnt];

	/* Make copies of all samples */
	avail = sample_alloc(&w->pool, cpys, cnt);
	if (avail < cnt)
		warn("Pool underrun for node %s: avail=%u", node_name(n), avail);

	for (int i = 0; i < avail; i++) {
		sample_copy(cpys[i], smps[i]);

		cpys[i]->source = n;
		cpys[i]->id = n->id;
	}

	for (size_t i = 0; i < list_length(&connections); i++) {
		struct websocket_connection *c = list_at(&connections, i);

		if (c->node == n || c->node == NULL)
			websocket_connection_write(c, cpys, cnt);
	}

	sample_put_many(cpys, avail);

	return cnt;
}

int websocket_parse(struct node *n, json_t *cfg)
{
	struct websocket *w = n->_vd;
	int ret;

	size_t index;
	json_t *cfg_dests = NULL;
	json_t *cfg_dest;
	json_error_t err;

	list_init(&w->destinations);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o }", "destinations", &cfg_dests);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (cfg_dests) {
		if (!json_is_array(cfg_dests))
			error("The 'destinations' setting of node %s must be an array of URLs", node_name(n));

		json_array_foreach(cfg_dests, index, cfg_dest) {
			const char *uri, *prot, *ads, *path;

			uri = json_string_value(cfg_dest);
			if (!uri)
				error("The 'destinations' setting of node %s must be an array of URLs", node_name(n));

			struct websocket_destination *d = alloc(sizeof(struct websocket_destination));

			d->uri = strdup(uri);

			ret = lws_parse_uri(d->uri, &prot, &ads, &d->info.port, &path);
			if (ret)
				error("Failed to parse WebSocket URI: '%s'", uri);

			d->info.ssl_connection = !strcmp(prot, "https");
			d->info.address = strdup(ads);
			d->info.path    = strdup(path);
			d->info.host    = d->info.address;
			d->info.origin  = d->info.address;
			d->info.ietf_version_or_minus_one = -1;
			d->info.protocol = "live";

			list_push(&w->destinations, d);
		}
	}

	return 0;
}

char * websocket_print(struct node *n)
{
	struct websocket *w = n->_vd;

	char *buf = NULL;

	buf = strcatf(&buf, "destinations=[ ");

	for (size_t i = 0; i < list_length(&w->destinations); i++) {
		struct websocket_destination *d = list_at(&w->destinations, i);

		buf = strcatf(&buf, "%s://%s:%d/%s ",
			d->info.ssl_connection ? "wss" : "ws",
			d->info.address,
			d->info.port,
			d->info.path
		);
	}

	buf = strcatf(&buf, "]");

	return buf;
}

int websocket_fd(struct node *n)
{
	struct websocket *w = n->_vd;

	return queue_signalled_fd(&w->queue);
}

static struct plugin p = {
	.name		= "websocket",
	.description	= "Send and receive samples of a WebSocket connection (libwebsockets)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct websocket),
		.init		= websocket_init,
		.deinit		= websocket_deinit,
		.start		= websocket_start,
		.stop		= websocket_stop,
		.destroy	= websocket_destroy,
		.read		= websocket_read,
		.write		= websocket_write,
		.print		= websocket_print,
		.parse		= websocket_parse,
		.fd		= websocket_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
