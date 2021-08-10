/** Node type: ZeroMQ
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstring>
#include <zmq.h>

#if ZMQ_VERSION_MAJOR < 4 || (ZMQ_VERSION_MAJOR == 4 && ZMQ_VERSION_MINOR <= 1)
  #include <zmq_utils.h>
#endif

#include <villas/node_compat.hpp>
#include <villas/nodes/zeromq.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/queue.h>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static void *context;

/**  Read one event off the monitor socket; return value and address
 * by reference, if not null, and event number by value.
 *
 * @returnval  -1 In case of error. */
static
int get_monitor_event(void *monitor, int *value, char **address)
{
	/* First frame in message contains event number and value */
	zmq_msg_t msg;
	zmq_msg_init (&msg);
	if (zmq_msg_recv (&msg, monitor, 0) == -1)
		return -1; /* Interruped, presumably. */

	assert(zmq_msg_more (&msg));

	uint8_t *data = (uint8_t *) zmq_msg_data(&msg);
	uint16_t event = *(uint16_t *) (data);
	if (value)
		*value = *(uint32_t *) (data + 2);

	/* Second frame in message contains event address */
	zmq_msg_init(&msg);
	if (zmq_msg_recv(&msg, monitor, 0) == -1)
		return -1; /* Interruped, presumably. */

	assert(!zmq_msg_more(&msg));

	if (address) {
		uint8_t *data = (uint8_t *) zmq_msg_data(&msg);
		size_t size = zmq_msg_size(&msg);
		*address = (char *) malloc(size + 1);
		memcpy(*address, data, size);
		*address [size] = 0;
	}

	return event;
}

int villas::node::zeromq_reverse(NodeCompat *n)
{
	auto *z = n->getData<struct zeromq>();

	if (list_length(&z->out.endpoints) != 1 ||
	    list_length(&z->in.endpoints) != 1)
		return -1;

	char *subscriber = (char *) list_first(&z->in.endpoints);
	char *publisher =  (char *) list_first(&z->out.endpoints);

	list_set(&z->in.endpoints, 0, publisher);
	list_set(&z->out.endpoints, 0, subscriber);

	return 0;
}

int villas::node::zeromq_init(NodeCompat *n)
{
	int ret;
	auto *z = n->getData<struct zeromq>();

	z->out.bind = 1;
	z->in.bind = 0;

	z->curve.enabled = false;
	z->ipv6 = 0;

	z->in.pending = 0;
	z->out.pending = 0;

	ret = list_init(&z->in.endpoints);
	if (ret)
		return ret;

	ret = list_init(&z->out.endpoints);
	if (ret)
		return ret;

	z->formatter = nullptr;

	return 0;
}

static
int zeromq_parse_endpoints(json_t *json_ep, struct List *epl)
{
	json_t *json_val;
	size_t i;
	const char *ep;

	switch (json_typeof(json_ep)) {
		case JSON_ARRAY:
			json_array_foreach(json_ep, i, json_val) {
				ep = json_string_value(json_val);
				if (!ep)
					throw ConfigError(json_val, "node-config-node-publish", "All 'publish' settings must be strings");

				list_push(epl, strdup(ep));
			}
			break;

		case JSON_STRING:
			ep = json_string_value(json_ep);
			list_push(epl, strdup(ep));
			break;

		default:
			return -1;
	}

	return 0;
}

int villas::node::zeromq_parse(NodeCompat *n, json_t *json)
{
	auto *z = n->getData<struct zeromq>();

	int ret;
	const char *type = nullptr;
	const char *in_filter = nullptr;
	const char *out_filter = nullptr;

	json_error_t err;
	json_t *json_in_ep = nullptr;
	json_t *json_out_ep = nullptr;
	json_t *json_curve = nullptr;
	json_t *json_format = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: o, s?: s, s?: b }, s?: { s?: o, s?: s, s?: b }, s?: o, s?: s, s?: b, s?: o }",
		"in",
			"subscribe", &json_in_ep,
			"filter", &in_filter,
			"bind", &z->in.bind,
		"out",
			"publish", &json_out_ep,
			"filter", &out_filter,
			"bind", &z->out.bind,
		"curve", &json_curve,
		"pattern", &type,
		"ipv6", &z->ipv6,
		"format", &json_format
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-zeromq");

	z->in.filter = in_filter ? strdup(in_filter) : nullptr;
	z->out.filter = out_filter ? strdup(out_filter) : nullptr;

	/* Format */
	if (z->formatter)
		delete z->formatter;
	z->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("villas.binary");
	if (!z->formatter)
		throw ConfigError(json_format, "node-config-node-zeromq-format", "Invalid format configuration");

	if (json_out_ep) {
		ret = zeromq_parse_endpoints(json_out_ep, &z->out.endpoints);
		if (ret)
			throw ConfigError(json_out_ep, "node-config-node-zeromq-publish", "Failed to parse list of publish endpoints");
	}

	if (json_in_ep) {
		ret = zeromq_parse_endpoints(json_in_ep, &z->in.endpoints);
		if (ret)
			throw ConfigError(json_out_ep, "node-config-node-zeromq-subscribe", "Failed to parse list of subscribe endpoints");
	}

	if (json_curve) {
		const char *public_key, *secret_key;

		z->curve.enabled = true;

		ret = json_unpack_ex(json_curve, &err, 0, "{ s: s, s: s, s?: b }",
			"public_key", &public_key,
			"secret_key", &secret_key,
			"enabled", &z->curve.enabled
		);
		if (ret)
			throw ConfigError(json_curve, err, "node-config-node-zeromq-curve", "Failed to parse setting 'curve'");

		if (strlen(secret_key) != 40)
			throw ConfigError(json_curve, err, "node-config-node-zeromq-curve", "Setting 'curve.secret_key' must be a Z85 encoded CurveZMQ key");

		if (strlen(public_key) != 40)
			throw ConfigError(json_curve, err, "node-config-node-zeromq-curve", "Setting 'curve.public_key' must be a Z85 encoded CurveZMQ key");

		memcpy(z->curve.server.public_key, public_key, 41);
		memcpy(z->curve.server.secret_key, secret_key, 41);
	}

	/** @todo We should fix this. Its mostly done. */
	if (z->curve.enabled)
		throw ConfigError(json_curve, "node-config-zeromq-curve", "CurveZMQ support is currently broken");

	if (type) {
		if      (!strcmp(type, "pubsub"))
			z->pattern = zeromq::Pattern::PUBSUB;
#ifdef ZMQ_BUILD_DISH
		else if (!strcmp(type, "radiodish"))
			z->pattern = zeromq::Pattern::RADIODISH;
#endif
		else
			throw ConfigError(json, "node-config-node-zeromq-type", "Invalid type for ZeroMQ node: {}", n->getNameShort());
	}

	return 0;
}

char * villas::node::zeromq_print(NodeCompat *n)
{
	auto *z = n->getData<struct zeromq>();

	char *buf = nullptr;
	const char *pattern = nullptr;

	switch (z->pattern) {
		case zeromq::Pattern::PUBSUB:
			pattern = "pubsub";
			break;

#ifdef ZMQ_BUILD_DISH
		case zeromq::Pattern::RADIODISH:
			pattern = "radiodish";
			break;
#endif
	}

	strcatf(&buf, "pattern=%s, ipv6=%s, crypto=%s, in.bind=%s, out.bind=%s, in.subscribe=[ ",
		pattern,
		z->ipv6 ? "yes" : "no",
		z->curve.enabled ? "yes" : "no",
		z->in.bind ? "yes" : "no",
		z->out.bind ? "yes" : "no"
	);

	for (size_t i = 0; i < list_length(&z->in.endpoints); i++) {
		char *ep = (char *) list_at(&z->in.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "], out.publish=[ ");

	for (size_t i = 0; i < list_length(&z->out.endpoints); i++) {
		char *ep = (char *) list_at(&z->out.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "]");

	if (z->in.filter)
		strcatf(&buf, ", in.filter=%s", z->in.filter);

	if (z->out.filter)
		strcatf(&buf, ", out.filter=%s", z->out.filter);

	return buf;
}

int villas::node::zeromq_check(NodeCompat *n)
{
	auto *z = n->getData<struct zeromq>();

	if (list_length(&z->in.endpoints) == 0 &&
	    list_length(&z->out.endpoints) == 0)
		return -1;

	return 0;
}

int villas::node::zeromq_type_start(villas::node::SuperNode *sn)
{
	context = zmq_ctx_new();

	return context == nullptr;
}

int villas::node::zeromq_type_stop()
{
	return zmq_ctx_term(context);
}

int villas::node::zeromq_start(NodeCompat *n)
{
	int ret;
	auto *z = n->getData<struct zeromq>();

	struct zeromq::Dir* dirs[] = { &z->out, &z->in };

	z->formatter->start(n->getInputSignals(false), ~(int) SampleFlags::HAS_OFFSET);

	switch (z->pattern) {
#ifdef ZMQ_BUILD_DISH
		case zeromq::Pattern::RADIODISH:
			z->in.socket   = zmq_socket(context, ZMQ_DISH);
			z->out.socket  = zmq_socket(context, ZMQ_RADIO);
			break;
#endif

		case zeromq::Pattern::PUBSUB:
			z->in.socket   = zmq_socket(context, ZMQ_SUB);
			z->out.socket  = zmq_socket(context, ZMQ_PUB);
			break;
	}

	if (!z->in.socket || !z->out.socket) {
		ret = -1;
		goto fail;
	}

	/* Join group */
	switch (z->pattern) {
#ifdef ZMQ_BUILD_DISH
		case zeromq::Pattern::RADIODISH:
			ret = zmq_join(z->in.socket, z->in.filter);
			break;
#endif

		case zeromq::Pattern::PUBSUB:
			ret = zmq_setsockopt(z->in.socket, ZMQ_SUBSCRIBE, z->in.filter, z->in.filter ? strlen(z->in.filter) : 0);
			break;

		default:
			ret = -1;
	}

	if (ret < 0)
		goto fail;

	if (z->curve.enabled) {
		/* Publisher has server role */
		ret = zmq_setsockopt(z->out.socket, ZMQ_CURVE_SECRETKEY, z->curve.server.secret_key, 41);
		if (ret)
			goto fail;

		ret = zmq_setsockopt(z->out.socket, ZMQ_CURVE_PUBLICKEY, z->curve.server.public_key, 41);
		if (ret)
			goto fail;

		int curve_server = 1;
		ret = zmq_setsockopt(z->out.socket, ZMQ_CURVE_SERVER, &curve_server, sizeof(curve_server));
		if (ret)
			goto fail;

		/* Create temporary client keys first */
		ret = zmq_curve_keypair(z->curve.client.public_key, z->curve.client.secret_key);
		if (ret)
			goto fail;

		/* Subscriber has client role */
		ret = zmq_setsockopt(z->in.socket, ZMQ_CURVE_SECRETKEY, z->curve.client.secret_key, 41);
		if (ret)
			goto fail;

		ret = zmq_setsockopt(z->in.socket, ZMQ_CURVE_PUBLICKEY, z->curve.client.public_key, 41);
		if (ret)
			goto fail;

		ret = zmq_setsockopt(z->in.socket, ZMQ_CURVE_SERVERKEY, z->curve.server.public_key, 41);
		if (ret)
			goto fail;
	}

	for (auto d : dirs) {
		const char *mon_ep = d == &z->in ? "inproc://monitor-in" : "inproc://monitor-out";

		ret = zmq_setsockopt(d->socket, ZMQ_IPV6, &z->ipv6, sizeof(z->ipv6));
		if (ret)
			goto fail;

		int linger = 1000;
		ret = zmq_setsockopt(d->socket, ZMQ_LINGER, &linger, sizeof(linger));
		if (ret)
			goto fail;

		/* Monitor events on the server */
		ret = zmq_socket_monitor(d->socket, mon_ep, ZMQ_EVENT_ALL);
		if (ret < 0)
			goto fail;

		/* Create socket for collecting monitor events */
		d->mon_socket = zmq_socket(context, ZMQ_PAIR);
		if (!d->mon_socket) {
			ret = -1;
			goto fail;
		}

		/* Connect it to the inproc endpoints so they'll get events */
		ret = zmq_connect(d->mon_socket, mon_ep);
		if (ret < 0)
			goto fail;

		/* Connect / bind sockets to endpoints */
		for (size_t i = 0; i < list_length(&d->endpoints); i++) {
			char *ep = (char *) list_at(&d->endpoints, i);

			if (d->bind) {
				ret = zmq_bind(d->socket, ep);
				if (ret < 0)
					goto fail;
			}
			else {
				ret = zmq_connect(d->socket, ep);
				if (ret < 0)
					goto fail;
			}

			d->pending++;
		}
	}

	/* Wait for all connections to be connected */
	for (auto d : dirs) {
		while (d->pending > 0) {
			int evt = d->bind ? ZMQ_EVENT_LISTENING : ZMQ_EVENT_CONNECTED;

			ret = get_monitor_event(d->mon_socket, nullptr, nullptr);
			if (ret == evt)
				d->pending--;
		}
	}

#if defined(ZMQ_BUILD_DRAFT_API) && ZMQ_MAJOR_VERSION >= 4 && ZMQ_MINOR_VERSION >= 2 && ZMQ_MINOR_VERSION >= 3
	if (z->curve.enabled) {
		ret = get_monitor_event(z->in.mon_socket, nullptr, nullptr);
		return ret == ZMQ_EVENT_HANDSHAKE_SUCCEEDED;
	}
	else
		return 0; /* The handshake events are only emitted for CurveZMQ sessions. */
#else
	return 0;
#endif

fail:
	n->logger->info("Failed to start: {}", zmq_strerror(errno));

	return ret;
}

int villas::node::zeromq_stop(NodeCompat *n)
{
	int ret;
	auto *z = n->getData<struct zeromq>();

	struct zeromq::Dir* dirs[] = { &z->out, &z->in };

	for (auto d : dirs) {
		ret = zmq_close(d->socket);
		if (ret)
			return ret;

		ret = zmq_close(d->mon_socket);
		if (ret)
			return ret;
	}

	return 0;
}

int villas::node::zeromq_destroy(NodeCompat *n)
{
	int ret;
	auto *z = n->getData<struct zeromq>();

	if (z->in.filter)
		free(z->in.filter);

	if (z->out.filter)
		free(z->out.filter);

	ret = list_destroy(&z->out.endpoints, nullptr, true);
	if (ret)
		return ret;

	if (z->formatter)
		delete z->formatter;

	return 0;
}

int villas::node::zeromq_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int recv, ret;
	auto *z = n->getData<struct zeromq>();

	zmq_msg_t m;

	ret = zmq_msg_init(&m);
	if (ret < 0)
		return ret;

	if (z->in.filter) {
		switch (z->pattern) {
			case zeromq::Pattern::PUBSUB:
				/* Discard envelope */
				zmq_recv(z->in.socket, nullptr, 0, 0);
				break;

			default: { }
		}
	}

	/* Receive payload */
	ret = zmq_msg_recv(&m, z->in.socket, 0);
	if (ret < 0)
		return ret;

	recv = z->formatter->sscan((const char *) zmq_msg_data(&m), zmq_msg_size(&m), nullptr, smps, cnt);

	ret = zmq_msg_close(&m);
	if (ret)
		return ret;

	return recv;
}

int villas::node::zeromq_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	auto *z = n->getData<struct zeromq>();

	size_t wbytes;
	zmq_msg_t m;

	char data[4096];

	ret = z->formatter->sprint(data, sizeof(data), &wbytes, smps, cnt);
	if (ret <= 0)
		return -1;

	ret = zmq_msg_init_size(&m, wbytes);

	if (z->out.filter) {
		switch (z->pattern) {
#ifdef ZMQ_BUILD_DISH
			case zeromq::Pattern::RADIODISH:
				ret = zmq_msg_set_group(&m, z->out.filter);
				if (ret < 0)
					goto fail;
				break;
#endif

			case zeromq::Pattern::PUBSUB: /* Send envelope */
				zmq_send(z->out.socket, z->out.filter, strlen(z->out.filter), ZMQ_SNDMORE);
				break;
		}
	}

	memcpy(zmq_msg_data(&m), data, wbytes);

	ret = zmq_msg_send(&m, z->out.socket, 0);
	if (ret < 0)
		goto fail;

	ret = zmq_msg_close(&m);
	if (ret < 0)
		return ret;

	return cnt;

fail:
	zmq_msg_close(&m);

	return ret;
}

int villas::node::zeromq_poll_fds(NodeCompat *n, int fds[])
{
	int ret;
	auto *z = n->getData<struct zeromq>();

	int fd;
	size_t len = sizeof(fd);

	ret = zmq_getsockopt(z->in.socket, ZMQ_FD, &fd, &len);
	if (ret)
		return ret;

	fds[0] = fd;

	return 1;
}

int villas::node::zeromq_netem_fds(NodeCompat *n, int fds[])
{
	int ret;
	auto *z = n->getData<struct zeromq>();

	int fd;
	size_t len = sizeof(fd);

	ret = zmq_getsockopt(z->out.socket, ZMQ_FD, &fd, &len);
	if (ret)
		return ret;

	fds[0] = fd;

	return 1;
}

static NodeCompatType p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "zeromq";
	p.description	= "ZeroMQ Distributed Messaging (libzmq)";
	p.vectorize	= 0;
	p.size		= sizeof(struct zeromq);
	p.type.start	= zeromq_type_start;
	p.type.stop	= zeromq_type_stop;
	p.init		= zeromq_init;
	p.destroy	= zeromq_destroy;
	p.check		= zeromq_check;
	p.parse		= zeromq_parse;
	p.print		= zeromq_print;
	p.start		= zeromq_start;
	p.stop		= zeromq_stop;
	p.read		= zeromq_read;
	p.write		= zeromq_write;
	p.reverse	= zeromq_reverse;
	p.poll_fds	= zeromq_poll_fds;
	p.netem_fds	= zeromq_netem_fds;

	static NodeCompatFactory ncp(&p);
}
