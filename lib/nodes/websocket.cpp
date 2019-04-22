/** Node type: Websockets (libwebsockets)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <libwebsockets.h>

#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/buffer.h>
#include <villas/plugin.h>
#include <villas/nodes/websocket.hpp>
#include <villas/format_type.h>
#include <villas/formats/msg_format.h>
#include <villas/super_node.h>

#define DEFAULT_WEBSOCKET_BUFFER_SIZE (1 << 12)

/* Private static storage */
static struct vlist connections = { .state = STATE_DESTROYED };	/**< List of active libwebsocket connections which receive samples from all nodes (catch all) */

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
		else if (c->mode == websocket_connection::mode::CLIENT && c->destination != nullptr)
			strcatf(&c->_name, "dest=%s:%d", c->destination->info.address, c->destination->info.port);

		if (c->node)
			strcatf(&c->_name, ", node=%s", node_name(c->node));

		strcatf(&c->_name, ", mode=%s", c->mode == websocket_connection::mode::CLIENT ? "client" : "server");
	}

	return c->_name;
}

static void websocket_destination_destroy(struct websocket_destination *d)
{
	free(d->uri);

	free((char *) d->info.path);
	free((char *) d->info.address);
}

static int websocket_connection_init(struct websocket_connection *c)
{
	int ret;

	c->_name = nullptr;

	ret = queue_init(&c->queue, DEFAULT_QUEUE_LENGTH, &memory_hugepage);
	if (ret)
		return ret;

	ret = io_init(&c->io, c->format, &c->node->in.signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&c->io);
	if (ret)
		return ret;

	ret = buffer_init(&c->buffers.recv, DEFAULT_WEBSOCKET_BUFFER_SIZE);
	if (ret)
		return ret;

	ret = buffer_init(&c->buffers.send, DEFAULT_WEBSOCKET_BUFFER_SIZE);
	if (ret)
		return ret;

	c->state = websocket_connection::state::INITIALIZED;

	return 0;
}

static int websocket_connection_destroy(struct websocket_connection *c)
{
	int ret;

	assert(c->state != websocket_connection::state::DESTROYED);

	if (c->_name)
		free(c->_name);

	/* Return all samples to pool */
	int avail;
	struct sample *smp;
	while ((avail = queue_pull(&c->queue, (void **) &smp)))
		sample_decref(smp);

	ret = queue_destroy(&c->queue);
	if (ret)
		return ret;

	ret = io_destroy(&c->io);
	if (ret)
		return ret;

	ret = buffer_destroy(&c->buffers.recv);
	if (ret)
		return ret;

	ret = buffer_destroy(&c->buffers.send);
	if (ret)
		return ret;

	c->wsi = nullptr;
	c->_name = nullptr;

	c->state = websocket_connection::state::DESTROYED;

	return 0;
}

static int websocket_connection_write(struct websocket_connection *c, struct sample *smps[], unsigned cnt)
{
	int pushed;

	if (c->state != websocket_connection::state::INITIALIZED)
		return -1;

	pushed = queue_push_many(&c->queue, (void **) smps, cnt);
	if (pushed < (int) cnt)
		warning("Queue overrun in WebSocket connection: %s", websocket_connection_name(c));

	sample_incref_many(smps, pushed);

	debug(LOG_WEBSOCKET | 10, "Enqueued %u samples to %s", pushed, websocket_connection_name(c));

	/* Client connections which are currently conecting don't have an associate c->wsi yet */
	if (c->wsi)
		web_callback_on_writable(web, c->wsi);

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
	struct websocket_connection *c = (struct websocket_connection *) user;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_ESTABLISHED:
			c->wsi = wsi;
			c->state = websocket_connection::state::ESTABLISHED;

			info("Established WebSocket connection: %s", websocket_connection_name(c));

			if (reason == LWS_CALLBACK_CLIENT_ESTABLISHED)
				c->mode = websocket_connection::mode::CLIENT;
			else {
				c->mode = websocket_connection::mode::SERVER;
				/* We use the URI to associate this connection to a node
				 * and choose a protocol.
				 *
				 * Example: ws://example.com/node_1.json
				 *   Will select the node with the name 'node_1'
				 *   and format 'json'.
				 */

				/* Get path of incoming request */
				char *node, *format, *lasts;
				char uri[64];

				lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
				if (strlen(uri) <= 0) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Invalid URL");
					warning("Failed to get request URI");
					return -1;
				}

				node = strtok_r(uri, "/.", &lasts);
				if (!node) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown node");
					warning("Failed to tokenize request URI");
					return -1;
				}

				format = strtok_r(nullptr, "", &lasts);
				if (!format)
					format = (char *) "villas.web";

				/* Search for node whose name matches the URI. */
				c->node = (struct node *) vlist_lookup(&p.node.instances, node);
				if (!c->node) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown node");
					warning("Failed to find node: node=%s", node);
					return -1;
				}

				c->format = format_type_lookup(format);
				if (!c->format) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown format");
					warning("Failed to find format: format=%s", format);
					return -1;
				}
			}

			ret = websocket_connection_init(c);
			if (ret) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Internal error");
				warning("Failed to intialize websocket connection: reason=%d", ret);
				return -1;
			}

			vlist_push(&connections, c);

			debug(LOG_WEBSOCKET | 10, "Initialized WebSocket connection: %s", websocket_connection_name(c));
			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			c->state = websocket_connection::state::ERROR;

			warning("Failed to establish WebSocket connection: %s, reason=%s", websocket_connection_name(c), in ? (char *) in : "unkown");

			return -1;

		case LWS_CALLBACK_CLOSED:
			debug(LOG_WEBSOCKET | 10, "Closed WebSocket connection: %s", websocket_connection_name(c));

			if (c->state != websocket_connection::state::SHUTDOWN) {
				/** @todo Attempt reconnect here */
			}

			if (connections.state == STATE_INITIALIZED)
				vlist_remove_all(&connections, c);

			if (c->state == websocket_connection::state::INITIALIZED)
				websocket_connection_destroy(c);

			if (c->mode == websocket_connection::mode::CLIENT)
				free(c);

			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			struct sample **smps = (struct sample **) alloca(cnt * sizeof(struct sample *));

			pulled = queue_pull_many(&c->queue, (void **) smps, cnt);
			if (pulled > 0) {
				size_t wbytes;
				io_sprint(&c->io, c->buffers.send.buf + LWS_PRE, c->buffers.send.size - LWS_PRE, &wbytes, smps, pulled);

				ret = lws_write(wsi, (unsigned char *) c->buffers.send.buf + LWS_PRE, wbytes, c->io.flags & IO_HAS_BINARY_PAYLOAD ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);

				sample_decref_many(smps, pulled);

				if (ret < 0)
					return ret;

				debug(LOG_WEBSOCKET | 10, "Send %d samples to connection: %s, bytes=%d", pulled, websocket_connection_name(c), ret);
			}

			if (queue_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);
			else if (c->state == websocket_connection::state::SHUTDOWN) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_GOINGAWAY, "Node stopped");
				return -1;
			}

			break;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
			if (lws_is_first_fragment(wsi))
				buffer_clear(&c->buffers.recv);

			ret = buffer_append(&c->buffers.recv, (char *) in, len);
			if (ret) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE, "Failed to process data");
				return -1;
			}

			/* We dont try to parse the frame yet, as we have to wait for the remaining fragments */
			if (lws_is_final_fragment(wsi)) {
				struct timespec ts_recv = time_now();
				struct node *n = c->node;

				int avail, enqueued;
				struct websocket *w = (struct websocket *) n->_vd;
				struct sample **smps = (struct sample **) alloca(cnt * sizeof(struct sample *));
				if (!smps) {
					warning("Failed to allocate memory for connection: %s", websocket_connection_name(c));
					break;
				}

				avail = sample_alloc_many(&w->pool, smps, cnt);
				if (avail < cnt)
					warning("Pool underrun for connection: %s", websocket_connection_name(c));

				recvd = io_sscan(&c->io, c->buffers.recv.buf, c->buffers.recv.len, nullptr, smps, avail);
				if (recvd < 0) {
					warning("Failed to parse sample data received on connection: %s", websocket_connection_name(c));
					break;
				}

				debug(LOG_WEBSOCKET | 10, "Received %d samples from connection: %s", recvd, websocket_connection_name(c));

				/* Set receive timestamp */
				for (int i = 0; i < recvd; i++) {
					smps[i]->ts.received = ts_recv;
					smps[i]->flags |= SAMPLE_HAS_TS_RECEIVED;
				}

				enqueued = queue_signalled_push_many(&w->queue, (void **) smps, recvd);
				if (enqueued < recvd)
					warning("Queue overrun in connection: %s", websocket_connection_name(c));

				/* Release unused samples back to pool */
				if (enqueued < avail)
					sample_decref_many(&smps[enqueued], avail - enqueued);

				buffer_clear(&c->buffers.recv);

				if (c->state == websocket_connection::state::SHUTDOWN) {
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

int websocket_type_start(struct super_node *sn)
{
	vlist_init(&connections);

	web = super_node_get_web(sn);
	if (web_get_state(web) != STATE_STARTED)
		return -1;

	return 0;
}

int websocket_start(struct node *n)
{
	int ret;
	struct websocket *w = (struct websocket *) n->_vd;

	ret = pool_init(&w->pool, DEFAULT_WEBSOCKET_QUEUE_LENGTH, SAMPLE_LENGTH(DEFAULT_WEBSOCKET_SAMPLE_LENGTH), &memory_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_WEBSOCKET_QUEUE_LENGTH, &memory_hugepage, 0);
	if (ret)
		return ret;

	for (size_t i = 0; i < vlist_length(&w->destinations); i++) {
		const char *format;
		struct websocket_destination *d = (struct websocket_destination *) vlist_at(&w->destinations, i);
		struct websocket_connection *c = (struct websocket_connection *) alloc(sizeof(struct websocket_connection));

		c->state = websocket_connection::state::CONNECTING;

		format = strchr(d->info.path, '.');
		if (format)
			format = format + 1; /* Removes "." */
		else
			format = "villas.web";

		c->format = format_type_lookup(format);
		if (!c->format)
			return -1;

		c->node = n;
		c->destination = d;

		d->info.context = web_get_context(web);
		d->info.vhost = web_get_vhost(web);
		d->info.userdata = c;

		lws_client_connect_via_info(&d->info);
	}

	return 0;
}

int websocket_stop(struct node *n)
{
	int ret, open_connections = 0;;
	struct websocket *w = (struct websocket *) n->_vd;

	for (size_t i = 0; i < vlist_length(&connections); i++) {
		struct websocket_connection *c = (struct websocket_connection *) vlist_at(&connections, i);

		if (c->node != n)
			continue;

		c->state = websocket_connection::state::SHUTDOWN;

		lws_callback_on_writable(c->wsi);
	}

	/* Count open connections belonging to this node */
	for (size_t i = 0; i < vlist_length(&connections); i++) {
		struct websocket_connection *c = (struct websocket_connection *) vlist_at(&connections, i);

		if (c->node == n)
			open_connections++;
	}

	if (open_connections > 0) {
		info("Waiting for shutdown of %u connections...", open_connections);
		sleep(1);
	}

	ret = queue_signalled_close(&w->queue);
	if (ret)
		return ret;

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
	struct websocket *w = (struct websocket *) n->_vd;
	int ret;

	ret = vlist_destroy(&w->destinations, (dtor_cb_t) websocket_destination_destroy, true);
	if (ret)
		return ret;

	return 0;
}

int websocket_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct websocket *w = (struct websocket *) n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
	if (avail < 0)
		return avail;

	sample_copy_many(smps, cpys, avail);
	sample_decref_many(cpys, avail);

	return avail;
}

int websocket_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct websocket *w = (struct websocket *) n->_vd;
	struct sample *cpys[cnt];

	/* Make copies of all samples */
	avail = sample_alloc_many(&w->pool, cpys, cnt);
	if (avail < (int) cnt)
		warning("Pool underrun for node %s: avail=%u", node_name(n), avail);

	sample_copy_many(cpys, smps, avail);

	for (size_t i = 0; i < vlist_length(&connections); i++) {
		struct websocket_connection *c = (struct websocket_connection *) vlist_at(&connections, i);

		if (c->node == n)
			websocket_connection_write(c, cpys, cnt);
	}

	sample_decref_many(cpys, avail);

	return cnt;
}

int websocket_parse(struct node *n, json_t *cfg)
{
	struct websocket *w = (struct websocket *) n->_vd;
	int ret;

	size_t i;
	json_t *json_dests = nullptr;
	json_t *json_dest;
	json_error_t err;

	vlist_init(&w->destinations);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o }", "destinations", &json_dests);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (json_dests) {
		if (!json_is_array(json_dests))
			error("The 'destinations' setting of node %s must be an array of URLs", node_name(n));

		json_array_foreach(json_dests, i, json_dest) {
			const char *uri, *prot, *ads, *path;

			uri = json_string_value(json_dest);
			if (!uri)
				error("The 'destinations' setting of node %s must be an array of URLs", node_name(n));

			struct websocket_destination *d = (struct websocket_destination *) alloc(sizeof(struct websocket_destination));

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

			vlist_push(&w->destinations, d);
		}
	}

	return 0;
}

char * websocket_print(struct node *n)
{
	struct websocket *w = (struct websocket *) n->_vd;

	char *buf = nullptr;

	buf = strcatf(&buf, "destinations=[ ");

	for (size_t i = 0; i < vlist_length(&w->destinations); i++) {
		struct websocket_destination *d = (struct websocket_destination *) vlist_at(&w->destinations, i);

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

int websocket_poll_fds(struct node *n, int fds[])
{
	struct websocket *w = (struct websocket *) n->_vd;

	fds[0] = queue_signalled_fd(&w->queue);

	return 1;
}

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
        if (plugins.state == STATE_DESTROYED)
	        vlist_init(&plugins);

	p.name		= "websocket";
	p.description	= "Send and receive samples of a WebSocket connection (libwebsockets)";
	p.type		= PLUGIN_TYPE_NODE;
	p.node.vectorize	= 0; /* unlimited */
	p.node.size		= sizeof(struct websocket);
	p.node.instances.state  = STATE_DESTROYED;
	p.node.type.start	= websocket_type_start;
	p.node.destroy		= websocket_destroy;
	p.node.parse		= websocket_parse;
	p.node.print		= websocket_print;
	p.node.start		= websocket_start;
	p.node.stop		= websocket_stop;
	p.node.read		= websocket_read;
	p.node.write		= websocket_write;
	p.node.poll_fds		= websocket_poll_fds;

	vlist_init(&p.node.instances);

	vlist_push(&plugins, &p);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
        if (plugins.state != STATE_DESTROYED)
                vlist_remove_all(&plugins, &p);
}
