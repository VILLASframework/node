/** Node type: nanomsg
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

#include <string.h>

#include "plugin.h"
#include "nodes/amqp.h"
#include "utils.h"
#include "io_format.h"

int amqp_reverse(struct node *n)
{
	struct amqp *a = n->_vd;

	if (list_length(&m->publisher.endpoints)  != 1 ||
	    list_length(&m->subscriber.endpoints) != 1)
		return -1;

	char *subscriber = list_first(&m->subscriber.endpoints);
	char *publisher = list_first(&m->publisher.endpoints);

	list_set(&m->subscriber.endpoints, 0, publisher);
	list_set(&m->publisher.endpoints, 0, subscriber);

	return 0;
}

static int amqp_parse_endpoints(struct list *l, json_t *cfg)
{
	const char *ep;

	size_t index;
	json_t *json_val;

	switch (json_typeof(cfg)) {
		case JSON_ARRAY:
			json_array_foreach(cfg, index, json_val) {
				ep = json_string_value(json_val);
				if (!ep)
					return -1;

				list_push(l, strdup(ep));
			}
			break;

		case JSON_STRING:
			ep = json_string_value(cfg);

			list_push(l, strdup(ep));
			break;

		default:
			return -1;
	}

	return 0;
}

int amqp_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct amqp *a = n->_vd;

	const char *format = "json";
	const char *uri = NULL;
	const char *exchange, *routing_key, *queue;

	json_error_t err;

	json_t *json_producer = NULL;
	json_t *json_consumer = NULL;

	/* Default values */
	amqp_default_connection_info(&a->connection_info);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: s, s: s, s: s, s?: s }",
		"uri", &uri,
		"exchange", &exchange,
		"routing_key", &routing_key,
		"queue", &queue,
		"format", &format
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (uri) {
		ret = amqp_parse_url(uri, &a->connection_info);
		if (ret != AMQP_STATUS_OK)
			error("Failed to parse URI of node %s", node_name(n));
	}



	m->format = io_format_lookup(format);
	if (!m->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	if (json_producer) {

	}

	if (json_consumer) {

	}

	return 0;
}

char * amqp_print(struct node *n)
{
	struct amqp *a = n->_vd;

	char *buf = NULL;

	strcatf(&buf, "format=%s, uri=amqp://%s:%s@%s:%d%s, exchange=%s, routing_key=%s, queue=%s", plugin_name(m->format)
		a->connection_info.user,
		a->connection_info.password,
		a->connection_info.host,
		a->connection_info.port,
		a->connection_info.vhost,
		(char *) a->producer.exchange.bytes,
		(char *) a->producer.routing_key.bytes,
		(char *) a->consumer.queue.bytes,
	);

	return buf;
}

int amqp_connect(amqp_connection_t *conn, amqp_socket_t *sock, struct amqp_connection_info *ci)
{
	int ret;

	*connection = amqp_new_connection();
	*socket = amqp_tcp_socket_new(conn);

	ret = amqp_socket_open(socket, ci->hostname, ci->port);
	if (ret != AMQP_STATUS_OK)
		return -1;

	ret = amqp_login(&connection, ci->vhost, 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, ci->user, ci->password);
	if  (ret != AMQP_STATUS_OK)
		return -2;

	amqp_channel_open(a->producer.connection, 1);

	return 0;
}

int amqp_start(struct node *n)
{
	int ret;
	struct amqp *a = n->_vd;

	/* Start producer */
	ret = amqp_connect(&a->producer.connection, &a->producer.socket, &a->connection_info);
	if (ret)
		return -1;

	/* Start consumer */
	ret = amqp_connect(&a->consumer.connection, &a->consumer.socket, &a->connection_info);
	if (ret)
		return -2;

	return 0;
}

int amqp_stop(struct node *n)
{
	int ret;
	struct amqp *a = n->_vd;

	/* Stop producer */
	amqp_channel_close(a->procuder.connection, 1, AMQP_REPLY_SUCCESS);
	amqp_connection_close(a->procuder.connection, AMQP_REPLY_SUCCESS);
	amqp_destroy_connection(a->procuder.connection);

	/* Stop consumer */
	amqp_channel_close(a->consumer.connection, 1, AMQP_REPLY_SUCCESS);
	amqp_connection_close(a->consumer.connection, AMQP_REPLY_SUCCESS);
	amqp_destroy_connection(a->consumer.connection);

	return 0;
}

int amqp_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct amqp *a = n->_vd;
	int bytes;
	char data[amqp_MAX_PACKET_LEN];

	/* Receive message */
        ret = amqp_basic_consume(a->consumer.connection, 1, a->consumer.queue, message, 0, 1, 0, amqp_empty_table);


	return io_format_sscan(m->format, data, bytes, NULL, smps, cnt, 0);
}

int amqp_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct amqp *a = n->_vd;

	size_t wbytes;

	char data[amqp_MAX_PACKET_LEN];

	ret = io_format_sprint(m->format, data, sizeof(data), &wbytes, smps, cnt, SAMPLE_HAS_ALL);
	if (ret <= 0)
		return -1;

	/* Send message */
	ret = amqp_basic_publish(a->producer.connection, 1,
		a->producer.exchange,
		a->producer.routing_key,
		0, 0, NULL, message_bytes);

	if (ret != AMQP_STATUS_OK)
		return -1;

	return cnt;
}

int amqp_fd(struct node *n)
{
	int ret;
	struct amqp *a = n->_vd;

	return amqp_socket_get_sockfd(a->consumer.connection);
}

static struct plugin p = {
	.name		= "amqp",
	.description	= "Advanced Message Queueing Protoocl (rabbitmq-c)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct nanomsg),
		.reverse	= amqp_reverse,
		.parse		= amqp_parse,
		.print		= amqp_print,
		.start		= amqp_start,
		.stop		= amqp_stop,
		.read		= amqp_read,
		.write		= amqp_write,
		.fd		= amqp_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
