/** Node type: nanomsg
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

#include <amqp_ssl_socket.h>
#include <amqp_tcp_socket.h>

#include <villas/plugin.h>
#include <villas/nodes/amqp.h>
#include <villas/utils.h>
#include <villas/format_type.h>

static void amqp_default_ssl_info(struct amqp_ssl_info *s)
{
	s->verify_peer = 1;
	s->verify_hostname = 1;
	s->client_key = NULL;
	s->client_cert = NULL;
	s->ca_cert = NULL;
}

static amqp_bytes_t amqp_bytes_strdup(const char *str)
{
	size_t len = strlen(str) + 1;
	amqp_bytes_t buf = amqp_bytes_malloc(len);

	memcpy(buf.bytes, str, len);

	return buf;
}

static amqp_connection_state_t amqp_connect(struct amqp_connection_info *ci, struct amqp_ssl_info *ssl)
{
	int ret;
	amqp_rpc_reply_t rep;
	amqp_connection_state_t conn;
	amqp_socket_t *sock;

	conn = amqp_new_connection();
	if (!conn)
		return NULL;

	if (ci->ssl) {
		sock = amqp_ssl_socket_new(conn);
		if (!sock)
			return NULL;

		amqp_ssl_socket_set_verify_peer(sock, ssl->verify_peer);
		amqp_ssl_socket_set_verify_hostname(sock, ssl->verify_hostname);

		if (ssl->ca_cert)
			amqp_ssl_socket_set_cacert(sock, ssl->ca_cert);

		if (ssl->client_key && ssl->client_cert)
			amqp_ssl_socket_set_key(sock, ssl->client_cert, ssl->client_key);
	}
	else {
		sock = amqp_tcp_socket_new(conn);
		if (!sock)
			return NULL;
	}

	ret = amqp_socket_open(sock, ci->host, ci->port);
	if (ret != AMQP_STATUS_OK)
		return NULL;

	rep = amqp_login(conn, ci->vhost, 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, ci->user, ci->password);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return NULL;

	amqp_channel_open(conn, 1);
	rep = amqp_get_rpc_reply(conn);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return NULL;

	return conn;
}

static int amqp_close(amqp_connection_state_t conn)
{
	amqp_rpc_reply_t rep;

	rep = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return -1;

	rep = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return -1;

	return 0;
}

int amqp_parse(struct node *n, json_t *json)
{
	int ret;
	struct amqp *a = n->_vd;

	int port = 5672;
	const char *format = "json";
	const char *uri = NULL;
	const char *host = "localhost";
	const char *vhost = "/";
	const char *username = "guest";
	const char *password = "guest";
	const char *exchange, *routing_key;

	json_error_t err;

	json_t *json_ssl = NULL;

	/* Default values */
	amqp_default_ssl_info(&a->ssl_info);
	amqp_default_connection_info(&a->connection_info);

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: s, s?: s, s?: s, s?: i, s: s, s: s, s?: s, s?: o }",
		"uri", &uri,
		"host", &host,
		"vhost", &vhost,
		"username", &username,
		"password", &password,
		"port", &port,
		"exchange", &exchange,
		"routing_key", &routing_key,
		"format", &format,
		"ssl", &json_ssl
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	a->exchange = amqp_bytes_strdup(exchange);
	a->routing_key = amqp_bytes_strdup(routing_key);

	if (uri)
		a->uri = strdup(uri);
	else
		a->uri = strf("amqp://%s:%s@%s:%d/%s", username, password, host, port, vhost);

	ret = amqp_parse_url(a->uri, &a->connection_info);

	if (ret != AMQP_STATUS_OK)
		error("Failed to parse URI '%s' of node %s", uri, node_name(n));

	if (json_ssl) {
		const char *ca_cert = NULL;
		const char *client_cert = NULL;
		const char *client_key = NULL;

		ret = json_unpack_ex(json_ssl, &err, 0, "{ s?: b, s?: b, s?: s, s?: s, s?: s }",
			"verify_peer", &a->ssl_info.verify_peer,
			"verify_hostname", &a->ssl_info.verify_hostname,
			"ca_cert", &ca_cert,
			"client_key", &client_key,
			"client_cert", &client_cert
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		if (ca_cert)
			a->ssl_info.ca_cert = strdup(ca_cert);

		if (client_cert)
			a->ssl_info.client_cert = strdup(client_cert);

		if (client_key)
			a->ssl_info.client_key = strdup(client_key);
	}

	a->format = format_type_lookup(format);
	if (!a->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	return 0;
}

char * amqp_print(struct node *n)
{
	struct amqp *a = n->_vd;

	char *buf = NULL;

	strcatf(&buf, "format=%s, uri=%s://%s:%s@%s:%d%s, exchange=%s, routing_key=%s",
		format_type_name(a->format),
		a->connection_info.ssl ? "amqps" : "amqp",
		a->connection_info.user,
		a->connection_info.password,
		a->connection_info.host,
		a->connection_info.port,
		a->connection_info.vhost,
		(char *) a->exchange.bytes,
		(char *) a->routing_key.bytes
	);

	if (a->connection_info.ssl) {
		strcatf(&buf, ", ssl_info.verify_peer=%s, ssl_info.verify_hostname=%s",
			a->ssl_info.verify_peer ? "true" : "false",
			a->ssl_info.verify_hostname ? "true" : "false"
		);

		if (a->ssl_info.ca_cert)
			strcatf(&buf, ", ssl_info.ca_cert=%s", a->ssl_info.ca_cert);

		if (a->ssl_info.client_cert)
			strcatf(&buf, ", ssl_info.client_cert=%s", a->ssl_info.client_cert);

		if (a->ssl_info.client_key)
			strcatf(&buf, ", ssl_info.client_key=%s", a->ssl_info.client_key);
	}

	return buf;
}

int amqp_start(struct node *n)
{
	int ret;
	struct amqp *a = n->_vd;

	amqp_bytes_t queue;
	amqp_rpc_reply_t rep;
	amqp_queue_declare_ok_t *r;

	ret = io_init(&a->io, a->format, &n->signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&a->io);
	if (ret)
		return ret;

	/* Connect producer */
	a->producer = amqp_connect(&a->connection_info, &a->ssl_info);
	if (!a->producer)
		return -1;

	/* Connect consumer */
	a->consumer = amqp_connect(&a->connection_info, &a->ssl_info);
	if (!a->consumer)
		return -1;

	/* Declare exchange */
	amqp_exchange_declare(a->producer, 1, a->exchange, amqp_cstring_bytes("direct"), 0, 0, 0, 0, amqp_empty_table);
	rep = amqp_get_rpc_reply(a->consumer);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return -1;

	/* Declare private queue */
	r = amqp_queue_declare(a->consumer, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
	rep = amqp_get_rpc_reply(a->consumer);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return -1;

	queue = amqp_bytes_malloc_dup(r->queue);
	if (queue.bytes == NULL)
		return -1;

	/* Bind queue to exchange */
	amqp_queue_bind(a->consumer, 1, queue, a->exchange, a->routing_key, amqp_empty_table);
	rep = amqp_get_rpc_reply(a->consumer);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return -1;

	/* Start consumer */
	amqp_basic_consume(a->consumer, 1, queue, amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
	rep = amqp_get_rpc_reply(a->consumer);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return -1;

	amqp_bytes_free(queue);

	return 0;
}

int amqp_stop(struct node *n)
{
	int ret;
	struct amqp *a = n->_vd;

	ret = amqp_close(a->consumer);
	if (ret)
		return ret;

	ret = amqp_close(a->producer);
	if (ret)
		return ret;

	ret = io_destroy(&a->io);
	if (ret)
		return ret;

	return 0;
}

int amqp_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct amqp *a = n->_vd;
	amqp_envelope_t env;
	amqp_rpc_reply_t rep;

	rep = amqp_consume_message(a->consumer, &env, NULL, 0);
	if (rep.reply_type != AMQP_RESPONSE_NORMAL)
		return -1;

	ret = io_sscan(&a->io, env.message.body.bytes, env.message.body.len, NULL, smps, cnt);

	amqp_destroy_envelope(&env);

	return ret;
}

int amqp_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct amqp *a = n->_vd;
	char data[1500];
	size_t wbytes;

	ret = io_sprint(&a->io, data, sizeof(data), &wbytes, smps, cnt);
	if (ret <= 0)
		return -1;

	amqp_bytes_t message = {
		.len = wbytes,
		.bytes = data
	};

	/* Send message */
	ret = amqp_basic_publish(a->producer, 1,
		a->exchange,
		a->routing_key,
		0, 0, NULL, message);

	if (ret != AMQP_STATUS_OK)
		return -1;

	return cnt;
}

int amqp_fd(struct node *n)
{
	struct amqp *a = n->_vd;

	amqp_socket_t *sock = amqp_get_socket(a->consumer);

	return amqp_socket_get_sockfd(sock);
}

int amqp_destroy(struct node *n)
{
	struct amqp *a = n->_vd;

	if (a->uri)
		free(a->uri);

	if (a->ssl_info.client_cert)
		free(a->ssl_info.client_cert);

	if (a->ssl_info.client_key)
		free(a->ssl_info.client_key);

	if (a->ssl_info.ca_cert)
		free(a->ssl_info.ca_cert);

	if (a->producer)
		amqp_destroy_connection(a->producer);

	if (a->consumer)
		amqp_destroy_connection(a->consumer);

	return 0;
}

static struct plugin p = {
	.name		= "amqp",
	.description	= "Advanced Message Queueing Protoocl (rabbitmq-c)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct amqp),
		.parse		= amqp_parse,
		.print		= amqp_print,
		.start		= amqp_start,
		.stop		= amqp_stop,
		.read		= amqp_read,
		.write		= amqp_write,
		.destroy	= amqp_destroy,
		.fd		= amqp_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
