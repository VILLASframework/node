/** Node type: Websockets (libwebsockets)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <signal.h>

#include <libwebsockets.h>

#include <villas/timing.h>
#include <villas/utils.hpp>
#include <villas/node.h>
#include <villas/nodes/websocket.hpp>
#include <villas/super_node.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

#define DEFAULT_WEBSOCKET_BUFFER_SIZE (1 << 12)

/* Private static storage */
static struct vlist connections;	/**< List of active libwebsocket connections which receive samples from all nodes (catch all) */

static villas::node::Web *web;
static villas::Logger logger = logging.get("websocket");

/* Forward declarations */
static struct vnode_type p;

static char * websocket_connection_name(struct websocket_connection *c)
{
	if (!c->_name) {
		if (c->wsi) {
			char name[128];
			char ip[128];

			lws_get_peer_addresses(c->wsi, lws_get_socket_fd(c->wsi), name, sizeof(name), ip, sizeof(ip));

			strcatf(&c->_name, "remote.ip=%s, remote.name=%s", ip, name);
		}
		else if (c->mode == websocket_connection::Mode::CLIENT && c->destination != nullptr)
			strcatf(&c->_name, "dest=%s:%d", c->destination->info.address, c->destination->info.port);

		if (c->node)
			strcatf(&c->_name, ", node=%s", node_name(c->node));

		strcatf(&c->_name, ", mode=%s", c->mode == websocket_connection::Mode::CLIENT ? "client" : "server");
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

	ret = queue_init(&c->queue, DEFAULT_QUEUE_LENGTH);
	if (ret)
		return ret;

	c->formatter->start(&c->node->in.signals, ~(int) SampleFlags::HAS_OFFSET);

	c->buffers.recv = new Buffer(DEFAULT_WEBSOCKET_BUFFER_SIZE);
	c->buffers.send = new Buffer(DEFAULT_WEBSOCKET_BUFFER_SIZE);

	if (!c->buffers.recv || !c->buffers.send)
		throw MemoryAllocationError();

	c->state = websocket_connection::State::INITIALIZED;

	return 0;
}

static int websocket_connection_destroy(struct websocket_connection *c)
{
	int ret;

	assert(c->state != websocket_connection::State::DESTROYED);

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

	delete c->formatter;
	delete c->buffers.recv;
	delete c->buffers.send;

	c->wsi = nullptr;
	c->_name = nullptr;

	c->state = websocket_connection::State::DESTROYED;

	return 0;
}

static int websocket_connection_write(struct websocket_connection *c, struct sample * const smps[], unsigned cnt)
{
	int pushed;

	if (c->state != websocket_connection::State::INITIALIZED)
		return -1;

	pushed = queue_push_many(&c->queue, (void **) smps, cnt);
	if (pushed < (int) cnt)
		c->node->logger->warn("Queue overrun in WebSocket connection: {}", websocket_connection_name(c));

	sample_incref_many(smps, pushed);

	c->node->logger->debug("Enqueued {} samples to {}", pushed, websocket_connection_name(c));

	/* Client connections which are currently conecting don't have an associate c->wsi yet */
	if (c->wsi)
		web->callbackOnWritable(c->wsi);

	return 0;
}

static void websocket_connection_close(struct websocket_connection *c, struct lws *wsi, enum lws_close_status status, const char *reason)
{
	lws_close_reason(wsi, status, (unsigned char *) reason, strlen(reason));

	c->node->logger->debug("Closing WebSocket connection with {}: status={}, reason={}", websocket_connection_name(c), status, reason);
}

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret, recvd, pulled, cnt = 128;
	struct websocket_connection *c = (struct websocket_connection *) user;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_ESTABLISHED:
			c->wsi = wsi;
			c->state = websocket_connection::State::ESTABLISHED;

			if (reason == LWS_CALLBACK_CLIENT_ESTABLISHED)
				c->mode = websocket_connection::Mode::CLIENT;
			else {
				c->mode = websocket_connection::Mode::SERVER;
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

					logger->warn("Failed to get request URI");

					return -1;
				}

				node = strtok_r(uri, "/.", &lasts);
				if (!node) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown node");
					logger->warn("Failed to tokenize request URI");
					return -1;
				}

				format = strtok_r(nullptr, "", &lasts);
				if (!format)
					format = (char *) "villas.web";

				/* Search for node whose name matches the URI. */
				c->node = p.instances.lookup(node);
				if (!c->node) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown node");
					logger->warn("Failed to find node: {}", node);
					return -1;
				}

				c->formatter = FormatFactory::make(format);
				if (!c->formatter) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown format");
					c->node->logger->warn("Failed to find format: format={}", format);
					return -1;
				}
			}

			ret = websocket_connection_init(c);
			if (ret) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Internal error");
				c->node->logger->warn("Failed to intialize WebSocket connection: reason={}", ret);
				return -1;
			}

			vlist_push(&connections, c);

			c->node->logger->info("Established WebSocket connection: {}", websocket_connection_name(c));

			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			c->state = websocket_connection::State::ERROR;

			logger->warn("Failed to establish WebSocket connection: reason={}", in ? (char *) in : "unknown");

			return -1;

		case LWS_CALLBACK_CLOSED:
			c->node->logger->debug("Closed WebSocket connection: {}", websocket_connection_name(c));

			if (c->state != websocket_connection::State::SHUTDOWN) {
				/** @todo Attempt reconnect here */
			}

			if (connections.state == State::INITIALIZED)
				vlist_remove_all(&connections, c);

			if (c->state == websocket_connection::State::INITIALIZED)
				websocket_connection_destroy(c);

			if (c->mode == websocket_connection::Mode::CLIENT)
				delete c;

			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			struct sample *smps[cnt];

			pulled = queue_pull_many(&c->queue, (void **) smps, cnt);
			if (pulled > 0) {
				size_t wbytes;
				c->formatter->sprint(c->buffers.send->data() + LWS_PRE, c->buffers.send->size() - LWS_PRE, &wbytes, smps, pulled);

				ret = lws_write(wsi, (unsigned char *) c->buffers.send->data() + LWS_PRE, wbytes, c->formatter->isBinaryPayload() ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);

				sample_decref_many(smps, pulled);

				if (ret < 0)
					return ret;

				c->node->logger->debug("Send {} samples to connection: {}, bytes={}", pulled, websocket_connection_name(c), ret);
			}

			if (queue_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);
			else if (c->state == websocket_connection::State::SHUTDOWN) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_GOINGAWAY, "Node stopped");
				return -1;
			}

			break;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
			if (lws_is_first_fragment(wsi))
				c->buffers.recv->clear();

			c->buffers.recv->append((char *) in, len);

			/* We dont try to parse the frame yet, as we have to wait for the remaining fragments */
			if (lws_is_final_fragment(wsi)) {
				struct timespec ts_recv = time_now();
				struct vnode *n = c->node;

				int avail, enqueued;
				struct websocket *w = (struct websocket *) n->_vd;
				struct sample *smps[cnt];

				avail = sample_alloc_many(&w->pool, smps, cnt);
				if (avail < cnt)
					c->node->logger->warn("Pool underrun for connection: {}", websocket_connection_name(c));

				recvd = c->formatter->sscan(c->buffers.recv->data(), c->buffers.recv->size(), nullptr, smps, avail);
				if (recvd < 0) {
					c->node->logger->warn("Failed to parse sample data received on connection: {}", websocket_connection_name(c));
					break;
				}

				c->node->logger->debug("Received {} samples from connection: {}", recvd, websocket_connection_name(c));

				/* Set receive timestamp */
				for (int i = 0; i < recvd; i++) {
					smps[i]->ts.received = ts_recv;
					smps[i]->flags |= (int) SampleFlags::HAS_TS_RECEIVED;
				}

				enqueued = queue_signalled_push_many(&w->queue, (void **) smps, recvd);
				if (enqueued < recvd)
					c->node->logger->warn("Queue overrun in connection: {}", websocket_connection_name(c));

				/* Release unused samples back to pool */
				if (enqueued < avail)
					sample_decref_many(&smps[enqueued], avail - enqueued);

				c->buffers.recv->clear();

				if (c->state == websocket_connection::State::SHUTDOWN) {
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

int websocket_type_start(villas::node::SuperNode *sn)
{
	int ret;

	ret = vlist_init(&connections);
	if (ret)
		return ret;

	web = sn->getWeb();
	if (web->getState() != State::STARTED)
		return -1;

	return 0;
}

int websocket_start(struct vnode *n)
{
	int ret;
	struct websocket *w = (struct websocket *) n->_vd;

	ret = pool_init(&w->pool, DEFAULT_WEBSOCKET_QUEUE_LENGTH, SAMPLE_LENGTH(DEFAULT_WEBSOCKET_SAMPLE_LENGTH));
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_WEBSOCKET_QUEUE_LENGTH);
	if (ret)
		return ret;

	for (size_t i = 0; i < vlist_length(&w->destinations); i++) {
		const char *format;
		auto *d = (struct websocket_destination *) vlist_at(&w->destinations, i);
		auto *c = new struct websocket_connection;
		if (!c)
			throw MemoryAllocationError();

		c->state = websocket_connection::State::CONNECTING;

		format = strchr(d->info.path, '.');
		if (format)
			format = format + 1; /* Removes "." */
		else
			format = "villas.web";

		c->formatter = FormatFactory::make(format);
		if (!c->formatter)
			return -1;

		c->node = n;
		c->destination = d;

		d->info.context = web->getContext();
		d->info.vhost = web->getVHost();
		d->info.userdata = c;

		lws_client_connect_via_info(&d->info);
	}

	return 0;
}

int websocket_stop(struct vnode *n)
{
	int ret, open_connections = 0;;
	struct websocket *w = (struct websocket *) n->_vd;

	for (size_t i = 0; i < vlist_length(&connections); i++) {
		struct websocket_connection *c = (struct websocket_connection *) vlist_at(&connections, i);

		if (c->node != n)
			continue;

		c->state = websocket_connection::State::SHUTDOWN;

		lws_callback_on_writable(c->wsi);
	}

	/* Count open connections belonging to this node */
	for (size_t i = 0; i < vlist_length(&connections); i++) {
		struct websocket_connection *c = (struct websocket_connection *) vlist_at(&connections, i);

		if (c->node == n)
			open_connections++;
	}

	if (open_connections > 0) {
		n->logger->info("Waiting for shutdown of {} connections...", open_connections);
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

int websocket_destroy(struct vnode *n)
{
	struct websocket *w = (struct websocket *) n->_vd;
	int ret;

	ret = vlist_destroy(&w->destinations, (dtor_cb_t) websocket_destination_destroy, true);
	if (ret)
		return ret;

	return 0;
}

int websocket_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
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

int websocket_write(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	int avail;

	struct websocket *w = (struct websocket *) n->_vd;
	struct sample *cpys[cnt];

	/* Make copies of all samples */
	avail = sample_alloc_many(&w->pool, cpys, cnt);
	if (avail < (int) cnt)
		n->logger->warn("Pool underrun: avail={}", avail);

	sample_copy_many(cpys, smps, avail);

	for (size_t i = 0; i < vlist_length(&connections); i++) {
		struct websocket_connection *c = (struct websocket_connection *) vlist_at(&connections, i);

		if (c->node == n)
			websocket_connection_write(c, cpys, cnt);
	}

	sample_decref_many(cpys, avail);

	return cnt;
}

int websocket_parse(struct vnode *n, json_t *json)
{
	struct websocket *w = (struct websocket *) n->_vd;
	int ret;

	size_t i;
	json_t *json_dests = nullptr;
	json_t *json_dest;
	json_error_t err;

	ret = vlist_init(&w->destinations);
	if (ret)
		return ret;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o }", "destinations", &json_dests);
	if (ret)
		throw ConfigError(json, err, "node-config-node-websocket");

	if (json_dests) {
		if (!json_is_array(json_dests))
			throw ConfigError(json_dests, err, "node-config-node-websocket-destinations", "The 'destinations' setting must be an array of URLs");

		json_array_foreach(json_dests, i, json_dest) {
			const char *uri, *prot, *ads, *path;

			uri = json_string_value(json_dest);
			if (!uri)
				throw ConfigError(json_dest, err, "node-config-node-websocket-destinations", "The 'destinations' setting must be an array of URLs");

			auto *d = new struct websocket_destination;
			if (!d)
				throw MemoryAllocationError();

			memset(d, 0, sizeof(struct websocket_destination));

			d->uri = strdup(uri);

			ret = lws_parse_uri(d->uri, &prot, &ads, &d->info.port, &path);
			if (ret)
				throw ConfigError(json_dest, err, "node-config-node-websocket-destinations", "Failed to parse WebSocket URI: '{}'", uri);

			d->info.ssl_connection = !strcmp(prot, "https");
			d->info.address = strdup(ads);
			d->info.path    = strf("/%s", path);
			d->info.host    = d->info.address;
			d->info.origin  = d->info.address;
			d->info.ietf_version_or_minus_one = -1;
			d->info.protocol = "live";

			vlist_push(&w->destinations, d);
		}
	}

	return 0;
}

char * websocket_print(struct vnode *n)
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

int websocket_poll_fds(struct vnode *n, int fds[])
{
	struct websocket *w = (struct websocket *) n->_vd;

	fds[0] = queue_signalled_fd(&w->queue);

	return 1;
}

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	p.name		= "websocket";
	p.description	= "Send and receive samples of a WebSocket connection (libwebsockets)";
	p.vectorize	= 0;
	p.size		= sizeof(struct websocket);
	p.type.start	= websocket_type_start;
	p.destroy	= websocket_destroy;
	p.parse		= websocket_parse;
	p.print		= websocket_print;
	p.start		= websocket_start;
	p.stop		= websocket_stop;
	p.read		= websocket_read;
	p.write		= websocket_write;
	p.poll_fds	= websocket_poll_fds;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
