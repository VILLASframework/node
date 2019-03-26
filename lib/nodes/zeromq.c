/** Node type: ZeroMQ
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

#include <string.h>
#include <zmq.h>

#if ZMQ_VERSION_MAJOR < 4 || (ZMQ_VERSION_MAJOR == 4 && ZMQ_VERSION_MINOR <= 1)
  #include <zmq_utils.h>
#endif

#include <villas/nodes/zeromq.h>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/queue.h>
#include <villas/plugin.h>
#include <villas/format_type.h>

static void *context;

#if defined(ZMQ_BUILD_DRAFT_API) && ZMQ_MAJOR_VERSION >= 4 && ZMQ_MINOR_VERSION >= 2 && ZMQ_MINOR_VERSION >= 3
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
#endif

int zeromq_reverse(struct node *n)
{
	struct zeromq *z = (struct zeromq *) n->_vd;

	if (vlist_length(&z->out.endpoints) != 1)
		return -1;

	char *subscriber = z->in.endpoint;
	char *publisher = vlist_first(&z->out.endpoints);

	z->in.endpoint = publisher;
	vlist_set(&z->out.endpoints, 0, subscriber);

	return 0;
}

int zeromq_parse(struct node *n, json_t *cfg)
{
	struct zeromq *z = (struct zeromq *) n->_vd;

	int ret;
	const char *ep = NULL;
	const char *type = NULL;
	const char *in_filter = NULL;
	const char *out_filter = NULL;
	const char *format = "villas.binary";

	size_t i;
	json_t *json_pub = NULL;
	json_t *json_curve = NULL;
	json_t *json_val;
	json_error_t err;

	vlist_init(&z->out.endpoints);

	z->curve.enabled = false;
	z->ipv6 = 0;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: { s?: s, s?: s }, s?: { s?: o, s?: s }, s?: o, s?: s, s?: b, s?: s }",
		"in",
			"subscribe", &ep,
			"filter", &in_filter,
		"out",
			"publish", &json_pub,
			"filter", &out_filter,
		"curve", &json_curve,
		"pattern", &type,
		"ipv6", &z->ipv6,
		"format", &format
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	z->in.endpoint = ep ? strdup(ep) : NULL;
	z->in.filter = in_filter ? strdup(in_filter) : NULL;
	z->out.filter = out_filter ? strdup(out_filter) : NULL;

	z->format = format_type_lookup(format);
	if (!z->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	if (json_pub) {
		switch (json_typeof(json_pub)) {
			case JSON_ARRAY:
				json_array_foreach(json_pub, i, json_val) {
					ep = json_string_value(json_val);
					if (!ep)
						error("All 'publish' settings must be strings");

					vlist_push(&z->out.endpoints, strdup(ep));
				}
				break;

			case JSON_STRING:
				ep = json_string_value(json_pub);

				vlist_push(&z->out.endpoints, strdup(ep));

				break;

			default:
				error("Invalid type for ZeroMQ publisher setting");
		}
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
			z->pattern = ZEROMQ_PATTERN_PUBSUB;
#ifdef ZMQ_BUILD_DISH
		else if (!strcmp(type, "radiodish"))
			z->pattern = ZEROMQ_PATTERN_RADIODISH;
#endif
		else
			error("Invalid type for ZeroMQ node: %s", node_name_short(n));
	}

	return 0;
}

char * zeromq_print(struct node *n)
{
	struct zeromq *z = (struct zeromq *) n->_vd;

	char *buf = NULL;
	char *pattern = NULL;

	switch (z->pattern) {
		case ZEROMQ_PATTERN_PUBSUB: pattern = "pubsub"; break;
#ifdef ZMQ_BUILD_DISH
		case ZEROMQ_PATTERN_RADIODISH: pattern = "radiodish"; break;
#endif
	}

	strcatf(&buf, "format=%s, pattern=%s, ipv6=%s, crypto=%s, in.subscribe=%s, out.publish=[ ",
		format_type_name(z->format),
		pattern,
		z->ipv6 ? "yes" : "no",
		z->curve.enabled ? "yes" : "no",
		z->in.endpoint ? z->in.endpoint : ""
	);

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

int zeromq_type_start(struct super_node *sn)
{
	context = zmq_ctx_new();

	return context == NULL;
}

int zeromq_type_stop()
{
	return zmq_ctx_term(context);
}

int zeromq_start(struct node *n)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	ret = io_init(&z->io, z->format, &n->in.signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&z->io);
	if (ret)
		return ret;

	switch (z->pattern) {
#ifdef ZMQ_BUILD_DISH
		case ZEROMQ_PATTERN_RADIODISH:
			z->in.socket = zmq_socket(context, ZMQ_DISH);
			z->out.socket  = zmq_socket(context, ZMQ_RADIO);
			break;
#endif

		case ZEROMQ_PATTERN_PUBSUB:
			z->in.socket = zmq_socket(context, ZMQ_SUB);
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
		case ZEROMQ_PATTERN_RADIODISH:
			ret = zmq_join(z->in.socket, z->in.filter);
			break;
#endif

		case ZEROMQ_PATTERN_PUBSUB:
			ret = zmq_setsockopt(z->in.socket, ZMQ_SUBSCRIBE, z->in.filter, z->in.filter ? strlen(z->in.filter) : 0);
			break;

		default:
			ret = -1;
	}

	if (ret < 0)
		goto fail;

	ret = zmq_setsockopt(z->out.socket, ZMQ_IPV6, &z->ipv6, sizeof(z->ipv6));
	if (ret)
		goto fail;

	ret = zmq_setsockopt(z->in.socket, ZMQ_IPV6, &z->ipv6, sizeof(z->ipv6));
	if (ret)
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

#if defined(ZMQ_BUILD_DRAFT_API) && ZMQ_MAJOR_VERSION >= 4 && ZMQ_MINOR_VERSION >= 2 && ZMQ_MINOR_VERSION >= 3
	/* Monitor handshake events on the server */
	ret = zmq_socket_monitor(z->in.socket, "inproc://monitor-server", ZMQ_EVENT_HANDSHAKE_SUCCEEDED | ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL | ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL | ZMQ_EVENT_HANDSHAKE_FAILED_AUTH);
	if (ret < 0)
		goto fail;

	/* Create socket for collecting monitor events */
	z->in.mon_socket = zmq_socket(context, ZMQ_PAIR);
	if (!z->in.mon_socket) {
		ret = -1;
		goto fail;
	}

	/* Connect it to the inproc endpoints so they'll get events */
	ret = zmq_connect(z->in.mon_socket, "inproc://monitor-server");
	if (ret < 0)
		goto fail;
#endif

	/* Spawn server for publisher */
	for (size_t i = 0; i < vlist_length(&z->out.endpoints); i++) {
		char *ep = (char *) vlist_at(&z->out.endpoints, i);

		ret = zmq_bind(z->out.socket, ep);
		if (ret < 0)
			goto fail;
	}

	/* Connect subscribers to server socket */
	if (z->in.endpoint) {
		ret = zmq_connect(z->in.socket, z->in.endpoint);
		if (ret < 0) {
			info("Failed to bind ZeroMQ socket: endpoint=%s, error=%s", z->in.endpoint, zmq_strerror(errno));
			return ret;
		}
	}

#if defined(ZMQ_BUILD_DRAFT_API) && ZMQ_MAJOR_VERSION >= 4 && ZMQ_MINOR_VERSION >= 2 && ZMQ_MINOR_VERSION >= 3
	if (z->curve.enabled) {
		ret = get_monitor_event(z->in.mon_socket, NULL, NULL);
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

int zeromq_stop(struct node *n)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	ret = zmq_close(z->in.socket);
	if (ret)
		return ret;

#if defined(ZMQ_BUILD_DRAFT_API) && ZMQ_MAJOR_VERSION >= 4 && ZMQ_MINOR_VERSION >= 2 && ZMQ_MINOR_VERSION >= 3
	ret = zmq_close(z->in.mon_socket);
	if (ret)
		return ret;
#endif

	ret = io_destroy(&z->io);
	if (ret)
		return ret;

	return zmq_close(z->out.socket);
}

int zeromq_destroy(struct node *n)
{
	int ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	if (z->in.filter)
		free(z->in.filter);

	if (z->out.filter)
		free(z->out.filter);

	ret = vlist_destroy(&z->out.endpoints, NULL, true);
	if (ret)
		return ret;

	return 0;
}

int zeromq_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int recv, ret;
	struct zeromq *z = (struct zeromq *) n->_vd;

	zmq_msg_t m;

	ret = zmq_msg_init(&m);
	if (ret < 0)
		return ret;

	if (z->in.filter) {
		switch (z->pattern) {
			case ZEROMQ_PATTERN_PUBSUB:
				/* Discard envelope */
				zmq_recv(z->in.socket, NULL, 0, 0);
				break;

			default: { }
		}
	}

	/* Receive payload */
	ret = zmq_msg_recv(&m, z->in.socket, 0);
	if (ret < 0)
		return ret;

	recv = io_sscan(&z->io, zmq_msg_data(&m), zmq_msg_size(&m), NULL, smps, cnt);

	ret = zmq_msg_close(&m);
	if (ret)
		return ret;

	return recv;
}

int zeromq_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
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
			case ZEROMQ_PATTERN_RADIODISH:
				ret = zmq_msg_set_group(&m, z->out.filter);
				if (ret < 0)
					goto fail;
				break;
#endif

			case ZEROMQ_PATTERN_PUBSUB: /* Send envelope */
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

int zeromq_poll_fds(struct node *n, int fds[])
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

static struct plugin p = {
	.name		= "zeromq",
	.description	= "ZeroMQ Distributed Messaging (libzmq)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct zeromq),
		.type.start	= zeromq_type_start,
		.type.stop	= zeromq_type_stop,
		.reverse	= zeromq_reverse,
		.parse		= zeromq_parse,
		.print		= zeromq_print,
		.start		= zeromq_start,
		.stop		= zeromq_stop,
		.destroy	= zeromq_destroy,
		.read		= zeromq_read,
		.write		= zeromq_write,
		.poll_fds	= zeromq_poll_fds
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
