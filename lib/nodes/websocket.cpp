/** Node type: Websockets (libwebsockets)
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/timing.hpp>
#include <villas/utils.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/websocket.hpp>
#include <villas/super_node.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

#define DEFAULT_WEBSOCKET_BUFFER_SIZE (1 << 12)

/* Private static storage */
static std::list<struct websocket_connection *> connections;	/**< List of active libwebsocket connections which receive samples from all nodes (catch all) */
static std::mutex connections_lock;

static villas::node::Web *web;
static villas::Logger logger = logging.get("websocket");

/* Forward declarations */
static NodeCompatType p;
static NodeCompatFactory ncp(&p);

static
void websocket_destination_destroy(struct websocket_destination *d)
{
	free(d->uri);

	free((char *) d->info.path);
	free((char *) d->info.address);
}

static
int websocket_connection_init(struct websocket_connection *c)
{
	int ret;

	ret = queue_init(&c->queue, DEFAULT_QUEUE_LENGTH);
	if (ret)
		return ret;

	c->formatter->start(c->node->getInputSignals(false), ~(int) SampleFlags::HAS_OFFSET);

	c->buffers.recv = new Buffer(DEFAULT_WEBSOCKET_BUFFER_SIZE);
	c->buffers.send = new Buffer(DEFAULT_WEBSOCKET_BUFFER_SIZE);

	if (!c->buffers.recv || !c->buffers.send)
		throw MemoryAllocationError();

	c->state = websocket_connection::State::INITIALIZED;

	return 0;
}

static
int websocket_connection_destroy(struct websocket_connection *c)
{
	int ret;

	assert(c->state != websocket_connection::State::DESTROYED);

	/* Return all samples to pool */
	int avail;
	struct Sample *smp;

	while ((avail = queue_pull(&c->queue, (void **) &smp)))
		sample_decref(smp);

	ret = queue_destroy(&c->queue);
	if (ret)
		return ret;

	delete c->formatter;
	delete c->buffers.recv;
	delete c->buffers.send;

	c->wsi = nullptr;
	c->state = websocket_connection::State::DESTROYED;

	return 0;
}

static
int websocket_connection_write(struct websocket_connection *c, struct Sample * const smps[], unsigned cnt)
{
	int pushed;

	if (c->state != websocket_connection::State::ESTABLISHED)
		return -1;

	pushed = queue_push_many(&c->queue, (void **) smps, cnt);
	if (pushed < (int) cnt)
		c->node->logger->warn("Queue overrun in WebSocket connection: {}", *c);

	sample_incref_many(smps, pushed);

	c->node->logger->debug("Enqueued {} samples to {}", pushed, *c);

	/* Client connections which are currently connecting don't have an associate c->wsi yet */
	if (c->wsi)
		web->callbackOnWritable(c->wsi);
	else
		c->node->logger->warn("No WSI for conn?");

	return 0;
}

static
void websocket_connection_close(struct websocket_connection *c, struct lws *wsi, enum lws_close_status status, const char *reason)
{
	lws_close_reason(wsi, status, (unsigned char *) reason, strlen(reason));

	c->node->logger->debug("Closing WebSocket connection with {}: status={}, reason={}", *c, status, reason);

	c->state = websocket_connection::State::CLOSED;
}

int villas::node::websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret, recvd, pulled, cnt = 128;
	struct websocket_connection *c = (struct websocket_connection *) user;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_ESTABLISHED:
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
				auto *n = ncp.instances.lookup(node);
				if (!n) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown node");
					logger->warn("Failed to find node: {}", node);
					return -1;
				}

				c->node = dynamic_cast<NodeCompat *>(n);
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

			c->wsi = wsi;
			c->state = websocket_connection::State::ESTABLISHED;
			c->node->logger->info("Established WebSocket connection: {}", *c);

			{
				std::lock_guard guard(connections_lock);
				connections.push_back(c);
			}

			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			c->state = websocket_connection::State::ERROR;

			logger->warn("Failed to establish WebSocket connection: reason={}", in ? (char *) in : "unknown");

			return -1;

		case LWS_CALLBACK_CLOSED:
			c->state = websocket_connection::State::CLOSED;
			c->node->logger->debug("Closed WebSocket connection: {}", *c);

			if (c->state != websocket_connection::State::CLOSING) {
				/** @todo Attempt reconnect here */
			}

			{
				std::lock_guard guard(connections_lock);
				connections.remove(c);
			}

			ret = websocket_connection_destroy(c);
			if (ret)
				return ret;

			if (c->mode == websocket_connection::Mode::CLIENT)
				delete c;

			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			struct Sample *smps[cnt];

			pulled = queue_pull_many(&c->queue, (void **) smps, cnt);
			if (pulled > 0) {
				size_t wbytes;
				c->formatter->sprint(c->buffers.send->data() + LWS_PRE, c->buffers.send->size() - LWS_PRE, &wbytes, smps, pulled);

				auto isBinary = dynamic_cast<BinaryFormat*>(c->formatter) != nullptr;
				ret = lws_write(wsi, (unsigned char *) c->buffers.send->data() + LWS_PRE, wbytes, isBinary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);

				sample_decref_many(smps, pulled);

				if (ret < 0)
					return ret;

				c->node->logger->debug("Send {} samples to connection: {}, bytes={}", pulled, *c, ret);
			}

			if (queue_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);
			else if (c->state == websocket_connection::State::CLOSING) {
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
				auto *n = c->node;

				int avail, enqueued;
				auto *w = n->getData<struct websocket>();
				struct Sample *smps[cnt];

				avail = sample_alloc_many(&w->pool, smps, cnt);
				if (avail < cnt)
					c->node->logger->warn("Pool underrun for connection: {}", *c);

				recvd = c->formatter->sscan(c->buffers.recv->data(), c->buffers.recv->size(), nullptr, smps, avail);
				if (recvd < 0) {
					c->node->logger->warn("Failed to parse sample data received on connection: {}", *c);
					break;
				}

				c->node->logger->debug("Received {} samples from connection: {}", recvd, *c);

				/* Set receive timestamp */
				for (int i = 0; i < recvd; i++) {
					smps[i]->ts.received = ts_recv;
					smps[i]->flags |= (int) SampleFlags::HAS_TS_RECEIVED;
				}

				enqueued = queue_signalled_push_many(&w->queue, (void **) smps, recvd);
				if (enqueued < recvd)
					c->node->logger->warn("Queue overrun in connection: {}", *c);

				/* Release unused samples back to pool */
				if (enqueued < avail)
					sample_decref_many(&smps[enqueued], avail - enqueued);

				c->buffers.recv->clear();

				if (c->state == websocket_connection::State::CLOSING) {
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

int villas::node::websocket_type_start(villas::node::SuperNode *sn)
{
	web = sn->getWeb();
	if (!web->isEnabled())
		return -1;

	return 0;
}

int villas::node::websocket_init(NodeCompat *n)
{
	auto *w = n->getData<struct websocket>();

	w->wait = false;

	int ret = list_init(&w->destinations);
	if (ret)
		return ret;

	return 0;
}

int villas::node::websocket_start(NodeCompat *n)
{
	int ret;
	auto *w = n->getData<struct websocket>();

	ret = pool_init(&w->pool, DEFAULT_WEBSOCKET_QUEUE_LENGTH, SAMPLE_LENGTH(n->getInputSignals(false)->size()));
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_WEBSOCKET_QUEUE_LENGTH);
	if (ret)
		return ret;

	for (size_t i = 0; i < list_length(&w->destinations); i++) {
		const char *format;
		auto *d = (struct websocket_destination *) list_at(&w->destinations, i);
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

	/* Wait until all destinations are connected */
	if (w->wait) {
		unsigned connected = 0, total = list_length(&w->destinations);
		do {
			{
				std::lock_guard guard(connections_lock);

				connected = 0;
				for (auto *c : connections) {
					if (c->mode == websocket_connection::Mode::CLIENT &&
					c->state == websocket_connection::State::ESTABLISHED &&
					c->node == n)
						connected++;
				}
			}

			if (connected < total) {
				n->logger->info("Wait until all destinations are connected: pending={}", total - connected);
				sleep(1);
			}
		} while (connected < total);
	}

	return 0;
}

int villas::node::websocket_stop(NodeCompat *n)
{
	int ret;
	auto *w = n->getData<struct websocket>();

	unsigned open_connections;
	do {
		{
			std::lock_guard guard(connections_lock);

			open_connections = 0;
			for (auto *c : connections) {
				if (c->node == n) {
					if (c->state != websocket_connection::State::CLOSED) {
						open_connections++;

						c->state = websocket_connection::State::CLOSING;
						lws_callback_on_writable(c->wsi);
					}
				}
			}
		}

		if (open_connections > 0) {
			n->logger->info("Waiting for open connections to be closed: pending={}", open_connections);
			sleep(1);
		}
	} while (open_connections > 0);

	ret = queue_signalled_close(&w->queue);
	if (ret)
		return ret;

	return 0;
}

int villas::node::websocket_destroy(NodeCompat *n)
{
	auto *w = n->getData<struct websocket>();
	int ret;

	ret = queue_signalled_destroy(&w->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	ret = list_destroy(&w->destinations, (dtor_cb_t) websocket_destination_destroy, true);
	if (ret)
		return ret;

	return 0;
}

int villas::node::websocket_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int avail;

	auto *w = n->getData<struct websocket>();
	struct Sample *cpys[cnt];

	avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
	if (avail < 0)
		return avail;

	sample_copy_many(smps, cpys, avail);
	sample_decref_many(cpys, avail);

	return avail;
}

int villas::node::websocket_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int avail;

	auto *w = n->getData<struct websocket>();
	struct Sample *cpys[cnt];

	/* Make copies of all samples */
	avail = sample_alloc_many(&w->pool, cpys, cnt);
	if (avail < (int) cnt)
		n->logger->warn("Pool underrun: avail={}", avail);

	sample_copy_many(cpys, smps, avail);

	{
		std::lock_guard guard(connections_lock);
		for (auto *c : connections) {
			if (c->node == n)
				websocket_connection_write(c, cpys, cnt);
		}
	}

	sample_decref_many(cpys, avail);

	return cnt;
}

int villas::node::websocket_parse(NodeCompat *n, json_t *json)
{
	auto *w = n->getData<struct websocket>();
	int ret;

	size_t i;
	json_t *json_dests = nullptr;
	json_t *json_dest;
	json_error_t err;
	int wc = -1;


	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: b }",
		"destinations", &json_dests,
		"wait_connected", &wc
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-websocket");

	if (wc >= 0)
		w->wait = wc != 0;

	list_clear(&w->destinations);
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

			list_push(&w->destinations, d);
		}
	}

	return 0;
}

char * villas::node::websocket_print(NodeCompat *n)
{
	auto *w = n->getData<struct websocket>();

	char *buf = nullptr;

	buf = strcatf(&buf, "destinations=[ ");

	for (size_t i = 0; i < list_length(&w->destinations); i++) {
		struct websocket_destination *d = (struct websocket_destination *) list_at(&w->destinations, i);

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

int villas::node::websocket_poll_fds(NodeCompat *n, int fds[])
{
	auto *w = n->getData<struct websocket>();

	fds[0] = queue_signalled_fd(&w->queue);

	return 1;
}

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	p.name		= "websocket";
	p.description	= "Send and receive samples of a WebSocket connection (libwebsockets)";
	p.vectorize	= 0;
	p.size		= sizeof(struct websocket);
	p.type.start	= websocket_type_start;
	p.init		= websocket_init;
	p.destroy	= websocket_destroy;
	p.parse		= websocket_parse;
	p.print		= websocket_print;
	p.start		= websocket_start;
	p.stop		= websocket_stop;
	p.read		= websocket_read;
	p.write		= websocket_write;
	p.poll_fds	= websocket_poll_fds;
	p.flags		= (int) NodeFactory::Flags::REQUIRES_WEB;
}
