/** Node type: ZeroMQ
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

#include <zmq.h>

#include "nodes/zeromq.h"
#include "utils.h"
#include "queue.h"
#include "plugin.h"
#include "msg.h"

static void *context;

#ifdef ZMQ_BUILD_DRAFT_API
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
	struct zeromq *z = n->_vd;
	
	if (list_length(&z->publisher.endpoints) != 1)
		return -1;
	
	char *subscriber = z->subscriber.endpoint;
	char *publisher = list_first(&z->publisher.endpoints);
	
	z->subscriber.endpoint = publisher;
	list_set(&z->publisher.endpoints, 0, subscriber);

	return 0;
}

int zeromq_parse(struct node *n, config_setting_t *cfg)
{
	struct zeromq *z = n->_vd;
	
	const char *ep, *type, *filter;
	
	config_setting_t *cfg_pub, *cfg_curve;
	
	list_init(&z->publisher.endpoints);
	
	if (config_setting_lookup_string(cfg, "subscribe", &ep))
		z->subscriber.endpoint = strdup(ep);
	else
		z->subscriber.endpoint = NULL;
	
	cfg_pub = config_setting_lookup(cfg, "publish");
	if (cfg_pub) {
		switch (config_setting_type(cfg_pub)) {
			case CONFIG_TYPE_LIST:
			case CONFIG_TYPE_ARRAY:
				for (int j = 0; j < config_setting_length(cfg_pub); j++) {
					const char *ep = config_setting_get_string_elem(cfg_pub, j);
					
					list_push(&z->publisher.endpoints, strdup(ep));
				}
				break;

			case CONFIG_TYPE_STRING:
				ep = config_setting_get_string(cfg_pub);
				
				list_push(&z->publisher.endpoints, strdup(ep));
				
				break;

			default:
				cerror(cfg_pub, "Invalid type for ZeroMQ publisher setting");
		}
	}
	
	cfg_curve = config_setting_lookup(cfg, "curve");
	if (cfg_curve) {
		if (!config_setting_is_group(cfg_curve))
			cerror(cfg_curve, "The curve setting must be a group");
		
		const char *public_key, *secret_key;
		
		if (!config_setting_lookup_string(cfg_curve, "public_key", &public_key))
			cerror(cfg_curve, "Setting 'curve.public_key' is missing");

		if (!config_setting_lookup_string(cfg_curve, "secret_key", &secret_key))
			cerror(cfg_curve, "Setting 'curve.secret_key' is missing");

		if (!config_setting_lookup_bool(cfg_curve, "enabled", &z->curve.enabled))
			z->curve.enabled = true;

		if (strlen(secret_key) != 40)
			cerror(cfg_curve, "Setting 'curve.secret_key' must be a Z85 encoded CurveZMQ key");

		if (strlen(public_key) != 40)
			cerror(cfg_curve, "Setting 'curve.public_key' must be a Z85 encoded CurveZMQ key");

		strncpy(z->curve.server.public_key, public_key, 41);
		strncpy(z->curve.server.secret_key, secret_key, 41);
	}
	else
		z->curve.enabled = false;
	
	/** @todo We should fix this. Its mostly done. */
	if (z->curve.enabled)
		cerror(cfg_curve, "CurveZMQ support is currently broken");
	
	if (config_setting_lookup_string(cfg, "filter", &filter))
		z->filter = strdup(filter);
	else
		z->filter = NULL;
	
	if (config_setting_lookup_string(cfg, "pattern", &type)) {
		if      (!strcmp(type, "pubsub"))
			z->pattern = ZEROMQ_PATTERN_PUBSUB;
		else if (!strcmp(type, "radiodish"))
			z->pattern = ZEROMQ_PATTERN_RADIODISH;
		else
			cerror(cfg, "Invalid type for ZeroMQ node: %s", node_name_short(n));
	}
	
	if (!config_setting_lookup_bool(cfg, "ipv6", &z->ipv6))
		z->ipv6 = 0;

	return 0;
}

char * zeromq_print(struct node *n)
{
	struct zeromq *z = n->_vd;

	char *buf = NULL;
	char *pattern = NULL;

	switch (z->pattern) {
		case ZEROMQ_PATTERN_PUBSUB: pattern = "pubsub"; break;
		case ZEROMQ_PATTERN_RADIODISH: pattern = "radiodish"; break;
	}
	
	strcatf(&buf, "pattern=%s, ipv6=%s, crypto=%s, subscribe=%s, publish=[ ", pattern, z->ipv6 ? "yes" : "no", z->curve.enabled ? "yes" : "no", z->subscriber.endpoint);
	
	for (size_t i = 0; i < list_length(&z->publisher.endpoints); i++) {
		char *ep = list_at(&z->publisher.endpoints, i);
		
		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, " ]");
	
	if (z->filter)
		strcatf(&buf, ", filter=%s", z->filter);

	return buf;
}

int zeromq_init(struct super_node *sn)
{
	context = zmq_ctx_new();

	return context == NULL;
}

int zeromq_deinit()
{
	return zmq_ctx_term(context);
}

int zeromq_start(struct node *n)
{
	int ret;
	struct zeromq *z = n->_vd;
	
	switch (z->pattern) {
		case ZEROMQ_PATTERN_RADIODISH:
			z->subscriber.socket = zmq_socket(context, ZMQ_DISH);
			z->publisher.socket  = zmq_socket(context, ZMQ_RADIO);
			break;
		
		case ZEROMQ_PATTERN_PUBSUB:
			z->subscriber.socket = zmq_socket(context, ZMQ_SUB);
			z->publisher.socket  = zmq_socket(context, ZMQ_PUB);
			break;
	}
	
	if (!z->subscriber.socket || !z->publisher.socket) {
		ret = -1;
		goto fail;
	}
	
	/* Join group */
	switch (z->pattern) {
		case ZEROMQ_PATTERN_RADIODISH:
			ret = zmq_join(z->subscriber.socket, z->filter);
			break;
		
		case ZEROMQ_PATTERN_PUBSUB:
			ret = zmq_setsockopt(z->subscriber.socket, ZMQ_SUBSCRIBE, z->filter, z->filter ? strlen(z->filter) : 0);
			break;
			
		default:
			ret = -1;
	}
	
	if (ret < 0)
		goto fail;
	
	ret = zmq_setsockopt(z->publisher.socket, ZMQ_IPV6, &z->ipv6, sizeof(z->ipv6));
	if (ret)
		goto fail;

	ret = zmq_setsockopt(z->subscriber.socket, ZMQ_IPV6, &z->ipv6, sizeof(z->ipv6));
	if (ret)
		goto fail;

	if (z->curve.enabled) {
		/* Publisher has server role */
		ret = zmq_setsockopt(z->publisher.socket, ZMQ_CURVE_SECRETKEY, z->curve.server.secret_key, 41);
		if (ret)
			goto fail;
		
		ret = zmq_setsockopt(z->publisher.socket, ZMQ_CURVE_PUBLICKEY, z->curve.server.public_key, 41);
		if (ret)
			goto fail;
 
		int curve_server = 1;
		ret = zmq_setsockopt(z->publisher.socket, ZMQ_CURVE_SERVER, &curve_server, sizeof(curve_server));
		if (ret)
			goto fail;
	}
	
	if (z->curve.enabled) {
		/* Create temporary client keys first */
		ret = zmq_curve_keypair(z->curve.client.public_key, z->curve.client.secret_key);
		if (ret)
			goto fail;

		/* Subscriber has client role */
		ret = zmq_setsockopt(z->subscriber.socket, ZMQ_CURVE_SECRETKEY, z->curve.client.secret_key, 41);
		if (ret)
			goto fail;

		ret = zmq_setsockopt(z->subscriber.socket, ZMQ_CURVE_PUBLICKEY, z->curve.client.public_key, 41);
		if (ret)
			goto fail;

		ret = zmq_setsockopt(z->subscriber.socket, ZMQ_CURVE_SERVERKEY, z->curve.server.public_key, 41);
		if (ret)
			goto fail;
	}
	
#ifdef ZMQ_BUILD_DRAFT_API
	/* Monitor handshake events on the server */
	ret = zmq_socket_monitor(z->subscriber.socket, "inproc://monitor-server", ZMQ_EVENT_HANDSHAKE_SUCCEED | ZMQ_EVENT_HANDSHAKE_FAILED);
	if (ret < 0)
		goto fail;

	/* Create socket for collecting monitor events */
	z->subscriber.mon_socket = zmq_socket(context, ZMQ_PAIR);
	if (!z->subscriber.mon_socket) {
		ret = -1;
		goto fail;
	}

	/* Connect it to the inproc endpoints so they'll get events */
	ret = zmq_connect(z->subscriber.mon_socket, "inproc://monitor-server");
	if (ret < 0)
		goto fail;
#endif

	/* Spawn server for publisher */
	for (size_t i = 0; i < list_length(&z->publisher.endpoints); i++) {
		char *ep = list_at(&z->publisher.endpoints, i);
		
		ret = zmq_bind(z->publisher.socket, ep);
		if (ret)
			goto fail;
	}
	
	/* Connect subscribers to server socket */
	if (z->subscriber.endpoint) {
		ret = zmq_connect(z->subscriber.socket, z->subscriber.endpoint);
		if (ret) {
			info("Failed to bind ZeroMQ socket: endpoint=%s, error=%s", z->subscriber.endpoint, zmq_strerror(errno));
			return ret;
		}
	}

#ifdef ZMQ_BUILD_DRAFT_API
	if (z->curve.enabled) {
		ret = get_monitor_event(z->subscriber.mon_socket, NULL, NULL);
		return ret == ZMQ_EVENT_HANDSHAKE_SUCCEED;
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
	struct zeromq *z = n->_vd;
	
	ret = zmq_close(z->subscriber.socket);
	if (ret)
		return ret;
	
#ifdef ZMQ_BUILD_DRAFT_API
	ret = zmq_close(z->subscriber.mon_socket);
	if (ret)
		return ret;
#endif

	return zmq_close(z->publisher.socket);
}

int zeromq_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int recv, ret;
	struct zeromq *z = n->_vd;

	zmq_msg_t m;
	
	ret = zmq_msg_init(&m);
	if (ret < 0)
		return ret;
	
	if (z->filter) {
		switch (z->pattern) {
			case ZEROMQ_PATTERN_PUBSUB:
				/* Discard envelope */
				zmq_recv(z->subscriber.socket, NULL, 0, 0);
				break;
			
			default: { }
		}
	}
	
	/* Receive payload */
	ret = zmq_msg_recv(&m, z->subscriber.socket, 0);
	if (ret < 0)
		return ret;

	recv = msg_buffer_to_samples(smps, cnt, zmq_msg_data(&m), zmq_msg_size(&m));
	
	ret = zmq_msg_close(&m);
	if (ret)
		return ret;
	
	return recv;
}

int zeromq_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct zeromq *z = n->_vd;
	
	ssize_t sent;
	zmq_msg_t m;
	
	char data[1500];

	sent = msg_buffer_from_samples(smps, cnt, data, sizeof(data));
	if (sent < 0)
		return -1;

	ret = zmq_msg_init_size(&m, sent);
	
	if (z->filter) {
		switch (z->pattern) {
			case ZEROMQ_PATTERN_RADIODISH:
				ret = zmq_msg_set_group(&m, z->filter);
				if (ret < 0)
					goto fail;
				break;
			
			case ZEROMQ_PATTERN_PUBSUB: /* Send envelope */
				zmq_send(z->publisher.socket, z->filter, strlen(z->filter), ZMQ_SNDMORE);
				break;
		}
	}
	
	memcpy(zmq_msg_data(&m), data, sent);

	ret = zmq_msg_send(&m, z->publisher.socket, 0);
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

static struct plugin p = {
	.name		= "zeromq",
	.description	= "ZeroMQ Distributed Messaging",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct zeromq),
		.reverse	= zeromq_reverse,
		.parse		= zeromq_parse,
		.print		= zeromq_print,
		.start		= zeromq_start,
		.stop		= zeromq_stop,
		.init		= zeromq_init,
		.deinit		= zeromq_deinit,
		.read		= zeromq_read,
		.write		= zeromq_write,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)