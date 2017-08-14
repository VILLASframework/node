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
		strcatf(&c->_name, "%s (%s)", c->peer.name, c->peer.ip);

		if (c->node)
			strcatf(&c->_name, " for node %s", node_name(c->node));

		strcatf(&c->_name, " in %s mode", c->mode == WEBSOCKET_MODE_CLIENT ? "client" : "server");
	}

	return c->_name;
}

static int websocket_connection_init(struct websocket_connection *c, struct lws *wsi)
{
	int ret;

	lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), c->peer.name, sizeof(c->peer.name), c->peer.ip, sizeof(c->peer.ip));

	info("LWS: New connection %s", websocket_connection_name(c));

	c->state = STATE_INITIALIZED;
	c->wsi = wsi;

	/** @todo: We must find a better way to determine the buffer size */
	c->buflen = 1 << 12;
	c->buf = alloc(c->buflen);
	

	if (c->node) {
		struct websocket *w = c->node->_vd;

		list_push(&w->connections, c);
	}
	else
		list_push(&connections, c);

	ret = queue_signalled_init(&c->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
	if (ret)
		return ret;

	return 0;
}

static int websocket_connection_destroy(struct websocket_connection *c)
{
	int ret;

	if (c->state == STATE_DESTROYED)
		return 0;

	info("LWS: Connection %s closed", websocket_connection_name(c));

	if (c->node) {
		struct websocket *w = c->node->_vd;
		list_remove(&w->connections, c);
	}
	else
		list_remove(&connections, c);

	if (c->_name)
		free(c->_name);

	ret = queue_signalled_destroy(&c->queue);
	if (ret)
		return ret;

	if (c->buf)
		free(c->buf);
	
	c->state = STATE_DESTROYED;
	c->wsi = NULL;

	return ret;
}

static void websocket_destination_destroy(struct websocket_destination *d)
{
	free(d->uri);

	free((char *) d->info.path);
	free((char *) d->info.address);
}

static int websocket_connection_write(struct websocket_connection *c, struct sample *smps[], unsigned cnt)
{
	int ret;

	switch (c->state) {
		case STATE_INITIALIZED:
			c->state = STATE_STARTED;
			/* fall through */

		case STATE_STARTED:
			for (int i = 0; i < cnt; i++) {
				sample_get(smps[i]); /* increase reference count */

				ret = queue_signalled_push(&c->queue, (void **) smps[i]);
				if (ret != 1)
					warn("Queue overrun in websocket connection: %s", websocket_connection_name(c));
			}

			lws_callback_on_writable(c->wsi);
			break;

		default: { }
	}

	return 0;
}

static void websocket_connection_close(struct websocket_connection *c, struct lws *wsi, enum lws_close_status status, const char *reason)
{
	lws_close_reason(wsi, status, (unsigned char *) reason, strlen(reason));

	char *msg = strf("LWS: Closing connection");

	if (c)
		msg = strcatf(&msg, " with %s", websocket_connection_name(c));

	msg = strcatf(&msg, ": status=%u, reason=%s", status, reason);

	warn("%s", msg);

	free(msg);
}

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;
	struct websocket_connection *c = user;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			ret = websocket_connection_init(c, wsi);
			if (ret)
				return -1;
			
			c->format = io_format_lookup("villas");

			break;
			
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			warn("Failed to establish connection: %s", websocket_connection_name(c));
		
			break;

		case LWS_CALLBACK_ESTABLISHED:
			c->state = STATE_DESTROYED;
			c->mode = WEBSOCKET_MODE_SERVER;

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
				format = "villas";

			c->format = io_format_lookup(format);
			if (!c->format) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Invalid format");
				return -1;
			}

			ret = websocket_connection_init(c, wsi);
			if (ret)
				return -1;

			break;

		case LWS_CALLBACK_CLOSED:
			websocket_connection_destroy(c);

			if (c->mode == WEBSOCKET_MODE_CLIENT)
				free(c);

			return 0;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			if (c->node && c->node->state != STATE_STARTED) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_GOINGAWAY, "Node stopped");
				return -1;
			}

			size_t wbytes;
			int cnt = 256; //c->node ? c->node->vectorize : 1;
			int pulled;
			
			struct sample **smps = alloca(cnt * sizeof(struct sample *));

			pulled = queue_signalled_pull_many(&c->queue, (void **) smps, cnt);

			io_format_sprint(c->format, c->buf + LWS_PRE, c->buflen - LWS_PRE, &wbytes, smps, pulled, IO_FORMAT_ALL);

			sample_put_many(smps, pulled);

			ret = lws_write(wsi, (unsigned char *) c->buf + LWS_PRE, wbytes, c->format->flags & IO_FORMAT_BINARY ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
			if (ret < 0) {
				warn("Failed lws_write() for connection %s", websocket_connection_name(c));
				return -1;
			}
			
			if (c->state == STATE_STOPPED) {
				info("Closing connection %s", websocket_connection_name(c));
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_NORMAL, "Goodbye");
				return -1;
			}
		
			if (queue_signalled_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);
			
			return 0;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE: {
			if (c->format->flags & IO_FORMAT_BINARY && !lws_frame_is_binary(wsi)) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE, "Binary data expected");
				return -1;
			}

			if (len <= 0) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Invalid packet");
				return -1;
			}

			if (!c->node) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Catch-all connection can not receive.");
				return -1;
			}

			struct timespec ts_recv = time_now();
			int recvd;
			int cnt = 256; //c->node->vectorize;
			struct websocket *w = c->node->_vd;
			struct sample **smps = alloca(cnt * sizeof(struct sample *));

			ret = sample_alloc(&w->pool, smps, cnt);
			if (ret != 1) {
				warn("Pool underrun for connection: %s", websocket_connection_name(c));
				break;
			}

			recvd = io_format_sscan(c->format, in, len, NULL, smps, cnt, NULL);
			if (recvd < 0) {
				warn("Failed to parse sample data received on connection: %s", websocket_connection_name(c));
				break;
			}

			struct node *dest;

			for (int i = 0; i < recvd; i++) {
				/* Set receive timestamp */
				smps[i]->ts.received = ts_recv;

				/* Find destination node of this message */
				if (c->node)
					dest = c->node;
				else {
					dest = NULL;

					for (int i = 0; i < list_length(&p.node.instances); i++) {
						struct node *n = list_at(&p.node.instances, i);

						if (n->id == smps[i]->id) {
							dest = n;
							break;
						}
					}

					if (!dest)
						warn("Ignoring message due to invalid node id");
				}
			}

			ret = queue_signalled_push_many(&w->queue, (void **) smps, recvd);
			if (ret != 1) {
				warn("Queue overrun for connection %s", websocket_connection_name(c));
				break;
			}

			return 0;
		}

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

		c->state = STATE_STOPPED;

		lws_callback_on_writable(c->wsi);
	}

	/* Wait for all connections to be closed */
	while (list_length(&connections) > 0) {
		info("LWS: Waiting for connection shutdown");
		sched_yield();
		usleep(0.1 * 1e6);
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

	ret = queue_signalled_init(&w->queue, DEFAULT_WEBSOCKET_QUEUELEN, &memtype_hugepage);
	if (ret)
		return ret;

	for (int i = 0; i < list_length(&w->destinations); i++) {
		struct websocket_destination *d = list_at(&w->destinations, i);

		struct websocket_connection *c = alloc(sizeof(struct websocket_connection));

		c->state = STATE_DESTROYED;
		c->mode = WEBSOCKET_MODE_CLIENT;
		c->node = n;
		c->destination = d;

		d->info.context = web->context;
		d->info.vhost = web->vhost;
		d->info.userdata = c;

		lws_client_connect_via_info(&d->info);
	}

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

	/* Wait for all connections to be closed */
	while (list_length(&w->connections) > 0) {
		info("LWS: Waiting for connection shutdown");
		sched_yield();
		usleep(0.1 * 1e6);
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

	list_destroy(&w->connections, NULL, false);
	list_destroy(&w->destinations, (dtor_cb_t) websocket_destination_destroy, true);

	return 0;
}

int websocket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int avail;

	struct websocket *w = n->_vd;
	struct sample *cpys[cnt];

	do {
		avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
		if (avail < 0)
			return avail;
	} while (avail == 0);

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
	}

	for (size_t i = 0; i < list_length(&w->connections); i++) {
		struct websocket_connection *c = list_at(&w->connections, i);

		websocket_connection_write(c, cpys, cnt);
	}

	for (size_t i = 0; i < list_length(&connections); i++) {
		struct websocket_connection *c = list_at(&connections, i);

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

	list_init(&w->connections);
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
			d->info.host    = d->info.address;
			d->info.origin  = d->info.address;
			d->info.ietf_version_or_minus_one = -1;
			d->info.protocol = "live";

			ret = asprintf((char **) &d->info.path, "/%s", path);

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

		buf = strcatf(&buf, "%s://%s:%d%s ",
			d->info.ssl_connection ? "wss" : "ws",
			d->info.address,
			d->info.port,
			d->info.path
		);
	}

	buf = strcatf(&buf, "]");

	return buf;
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
		.parse		= websocket_parse
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
