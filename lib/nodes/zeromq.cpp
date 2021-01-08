/** Node type: ZeroMQ
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

#include <cstring>
#include <zmq.h>

#if ZMQ_VERSION_MAJOR < 4 || (ZMQ_VERSION_MAJOR == 4 && ZMQ_VERSION_MINOR <= 1)
  #include <zmq_utils.h>
#endif

#include <villas/nodes/zeromq.hpp>
#include <villas/node.h>
#include <villas/utils.hpp>
#include <villas/queue.h>
#include <villas/plugin.h>
#include <villas/format_type.h>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::utils;

static void *context;

/**  Read one event off the monitor socket; return value and address
 * by reference, if not null, and event number by value.
 *
 * @returnval  -1 In case of error. */
static int get_monitor_event(void *monitor, int *value, char **address)
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

int zeromq_reverse(struct vnode *n)
{
	struct zeromq *z = (struct zeromq *) n->_vd;

	if (vlist_length(&z->out.endpoints) != 1 ||
	    vlist_length(&z->in.endpoints) != 1)
		return -1;

	char *subscriber = (char *) vlist_first(&z->in.endpoints);
	char *publisher =  (char *) vlist_first(&z->out.endpoints);

	vlist_set(&z->in.endpoints, 0, publisher);
	vlist_set(&z->out.endpoints, 0, subscriber);

	return 0;
}

int zeromq_init(struct vnode *n)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	z->out.bind = 1;
	z->in.bind = 0;

	z->curve.enabled = false;
	z->ipv6 = 0;

	z->in.pending = 0;
	z->out.pending = 0;

	ret = vlist_init(&z->in.endpoints);
	if (ret)
		return ret;

	ret = vlist_init(&z->out.endpoints);
	if (ret)
		return ret;

	return 0;
}

int zeromq_parse_endpoints(json_t *json_ep, struct vlist *epl)
{
	json_t *json_val;
	size_t i;
	const char *ep;

	switch (json_typeof(json_ep)) {
		case JSON_ARRAY:
			json_array_foreach(json_ep, i, json_val) {
				ep = json_string_value(json_val);
				if (!ep)
					error("All 'publish' settings must be strings");

				vlist_push(epl, strdup(ep));
			}
			break;

		case JSON_STRING:
			ep = json_string_value(json_ep);
			vlist_push(epl, strdup(ep));
			break;

		default:
			return -1;
	}

	return 0;
}

int zeromq_parse(struct vnode *n, json_t *cfg)
{
	struct zeromq *z = (struct zeromq *) n->_vd;

	int ret;
	const char *type = nullptr;
	const char *in_filter = nullptr;
	const char *out_filter = nullptr;
	const char *format = "villas.binary";

	json_t *json_in_ep = nullptr;
	json_t *json_out_ep = nullptr;
	json_t *json_curve = nullptr;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: { s?: o, s?: s, s?: b }, s?: { s?: o, s?: s, s?: b }, s?: o, s?: s, s?: b, s?: s }",
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
		"format", &format
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	z->in.filter = in_filter ? strdup(in_filter) : nullptr;
	z->out.filter = out_filter ? strdup(out_filter) : nullptr;

	z->format = format_type_lookup(format);
	if (!z->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

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
			jerror(&err, "Failed to parse setting 'curve' of node %s", node_name(n));

		if (strlen(secret_key) != 40)
			error("Setting 'curve.secret_key' of node %s must be a Z85 encoded CurveZMQ key", node_name(n));

		if (strlen(public_key) != 40)
			error("Setting 'curve.public_key' of node %s must be a Z85 encoded CurveZMQ key", node_name(n));

		memcpy(z->curve.server.public_key, public_key, 41);
		memcpy(z->curve.server.secret_key, secret_key, 41);
	}

	/** @todo We should fix this. Its mostly done. */
	if (z->curve.enabled)
		error("CurveZMQ support is currently broken");

	if (type) {
		if      (!strcmp(type, "pubsub"))
			z->pattern = zeromq::Pattern::PUBSUB;
#ifdef ZMQ_BUILD_DISH
		else if (!strcmp(type, "radiodish"))
			z->pattern = zeromq::Pattern::RADIODISH;
#endif
		else
			error("Invalid type for ZeroMQ node: %s", node_name_short(n));
	}

	return 0;
}

char * zeromq_print(struct vnode *n)
{
	struct zeromq *z = (struct zeromq *) n->_vd;

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

	strcatf(&buf, "format=%s, pattern=%s, ipv6=%s, crypto=%s, in.bind=%s, out.bind=%s, in.subscribe=[ ",
		format_type_name(z->format),
		pattern,
		z->ipv6 ? "yes" : "no",
		z->curve.enabled ? "yes" : "no",
		z->in.bind ? "yes" : "no",
		z->out.bind ? "yes" : "no"
	);

	for (size_t i = 0; i < vlist_length(&z->in.endpoints); i++) {
		char *ep = (char *) vlist_at(&z->in.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "], out.publish=[ ");

	for (size_t i = 0; i < vlist_length(&z->out.endpoints); i++) {
		char *ep = (char *) vlist_at(&z->out.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "]");

	if (z->in.filter)
		strcatf(&buf, ", in.filter=%s", z->in.filter);

	if (z->out.filter)
		strcatf(&buf, ", out.filter=%s", z->out.filter);

	return buf;
}

int zeromq_check(struct vnode *n)
{
	struct zeromq *z = (struct zeromq *) n->_vd;

	if (vlist_length(&z->in.endpoints) == 0 &&
	    vlist_length(&z->out.endpoints) == 0)
		return -1;

	return 0;
}

int zeromq_type_start(villas::node::SuperNode *sn)
{
	context = zmq_ctx_new();

	return context == nullptr;
}

int zeromq_type_stop()
{
	return zmq_ctx_term(context);
}

int zeromq_start(struct vnode *n)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	struct zeromq::Dir* dirs[] = { &z->out, &z->in };

	ret = io_init(&z->io, z->format, &n->in.signals, (int) SampleFlags::HAS_ALL & ~(int) SampleFlags::HAS_OFFSET);
	if (ret)
		return ret;

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
		for (size_t i = 0; i < vlist_length(&d->endpoints); i++) {
			char *ep = (char *) vlist_at(&d->endpoints, i);

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
	info("Failed to start ZeroMQ node: %s, error=%s", node_name(n), zmq_strerror(errno));

	return ret;
}

int zeromq_stop(struct vnode *n)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	struct zeromq::Dir* dirs[] = { &z->out, &z->in };

	for (auto d : dirs) {
		ret = zmq_close(d->socket);
		if (ret)
			return ret;

		ret = zmq_close(d->mon_socket);
		if (ret)
			return ret;
	}

	ret = io_destroy(&z->io);
	if (ret)
		return ret;

	return 0;
}

int zeromq_destroy(struct vnode *n)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	if (z->in.filter)
		free(z->in.filter);

	if (z->out.filter)
		free(z->out.filter);

	ret = vlist_destroy(&z->out.endpoints, nullptr, true);
	if (ret)
		return ret;

	return 0;
}

int zeromq_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int recv, ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

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

	recv = io_sscan(&z->io, (const char *) zmq_msg_data(&m), zmq_msg_size(&m), nullptr, smps, cnt);

	ret = zmq_msg_close(&m);
	if (ret)
		return ret;

	return recv;
}

int zeromq_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	size_t wbytes;
	zmq_msg_t m;

	char data[4096];

	ret = io_sprint(&z->io, data, sizeof(data), &wbytes, smps, cnt);
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

int zeromq_poll_fds(struct vnode *n, int fds[])
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	int fd;
	size_t len = sizeof(fd);

	ret = zmq_getsockopt(z->in.socket, ZMQ_FD, &fd, &len);
	if (ret)
		return ret;

	fds[0] = fd;

	return 1;
}

int zeromq_netem_fds(struct vnode *n, int fds[])
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	int fd;
	size_t len = sizeof(fd);

	ret = zmq_getsockopt(z->out.socket, ZMQ_FD, &fd, &len);
	if (ret)
		return ret;

	fds[0] = fd;

	return 1;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "zeromq";
	p.description		= "ZeroMQ Distributed Messaging (libzmq)";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct zeromq);
	p.node.type.start	= zeromq_type_start;
	p.node.type.stop	= zeromq_type_stop;
	p.node.init		= zeromq_init;
	p.node.destroy		= zeromq_destroy;
	p.node.check		= zeromq_check;
	p.node.parse		= zeromq_parse;
	p.node.print		= zeromq_print;
	p.node.start		= zeromq_start;
	p.node.stop		= zeromq_stop;
	p.node.read		= zeromq_read;
	p.node.write		= zeromq_write;
	p.node.reverse		= zeromq_reverse;
	p.node.poll_fds		= zeromq_poll_fds;
	p.node.netem_fds	= zeromq_netem_fds;

	int ret = vlist_init(&p.node.instances);
	if (!ret)
		vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
