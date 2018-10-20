/** Node type: nanomsg
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <string.h>

#include <villas/plugin.h>
#include <villas/nodes/nanomsg.h>
#include <villas/utils.h>
#include <villas/format_type.h>

int nanomsg_reverse(struct node *n)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	if (list_length(&m->out.endpoints)  != 1 ||
	    list_length(&m->in.endpoints) != 1)
		return -1;

	char *subscriber = list_first(&m->in.endpoints);
	char *publisher = list_first(&m->out.endpoints);

	list_set(&m->in.endpoints, 0, publisher);
	list_set(&m->out.endpoints, 0, subscriber);

	return 0;
}

static int nanomsg_parse_endpoints(struct list *l, json_t *cfg)
{
	const char *ep;

	size_t i;
	json_t *json_val;

	switch (json_typeof(cfg)) {
		case JSON_ARRAY:
			json_array_foreach(cfg, i, json_val) {
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

int nanomsg_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	const char *format = "villas.binary";

	json_error_t err;

	json_t *json_out_endpoints = NULL;
	json_t *json_in_endpoints = NULL;

	list_init(&m->out.endpoints);
	list_init(&m->in.endpoints);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: { s?: o }, s?: { s?: o } }",
		"format", &format,
		"out",
			"endpoints", &json_out_endpoints,
		"in",
			"endpoints", &json_in_endpoints
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (json_out_endpoints) {
		ret = nanomsg_parse_endpoints(&m->out.endpoints, json_out_endpoints);
		if (ret < 0)
			error("Invalid type for 'publish' setting of node %s", node_name(n));
	}

	if (json_in_endpoints) {
		ret = nanomsg_parse_endpoints(&m->in.endpoints, json_in_endpoints);
		if (ret < 0)
			error("Invalid type for 'subscribe' setting of node %s", node_name(n));
	}

	m->format = format_type_lookup(format);
	if (!m->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	return 0;
}

char * nanomsg_print(struct node *n)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	char *buf = NULL;

	strcatf(&buf, "format=%s, in.endpoints=[ ", format_type_name(m->format));

	for (size_t i = 0; i < list_length(&m->in.endpoints); i++) {
		char *ep = (char *) list_at(&m->in.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "], out.endpoints=[ ");

	for (size_t i = 0; i < list_length(&m->out.endpoints); i++) {
		char *ep = (char *) list_at(&m->out.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "]");

	return buf;
}

int nanomsg_start(struct node *n)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	ret = io_init(&m->io, m->format, &n->signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&m->io);
	if (ret)
		return ret;

	ret = m->in.socket = nn_socket(AF_SP, NN_SUB);
	if (ret < 0) {
		warn("Failed to create nanomsg socket: node=%s, error=%s", node_name(n), nn_strerror(errno));
		return ret;
	}

	ret = m->out.socket = nn_socket(AF_SP, NN_PUB);
	if (ret < 0) {
		warn("Failed to create nanomsg socket: node=%s, error=%s", node_name(n), nn_strerror(errno));
		return ret;
	}

	/* Subscribe to all topics */
	ret = nn_setsockopt(ret = m->in.socket, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);
	if (ret < 0)
		return ret;

	/* Bind publisher to socket */
	for (size_t i = 0; i < list_length(&m->out.endpoints); i++) {
		char *ep = (char *) list_at(&m->out.endpoints, i);

		ret = nn_bind(m->out.socket, ep);
		if (ret < 0) {
			warn("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}

	/* Connect subscribers socket */
	for (size_t i = 0; i < list_length(&m->in.endpoints); i++) {
		char *ep = (char *) list_at(&m->in.endpoints, i);

		ret = nn_connect(m->in.socket, ep);
		if (ret < 0) {
			warn("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}

	return 0;
}

int nanomsg_stop(struct node *n)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	ret = nn_close(m->in.socket);
	if (ret < 0)
		return ret;

	ret = nn_close(m->out.socket);
	if (ret < 0)
		return ret;

	ret = io_destroy(&m->io);
	if (ret)
		return ret;

	return 0;
}

int nanomsg_type_stop()
{
	nn_term();

	return 0;
}

int nanomsg_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;
	int bytes;
	char data[NANOMSG_MAX_PACKET_LEN];

	/* Receive payload */
	bytes = nn_recv(m->in.socket, data, sizeof(data), 0);
	if (bytes < 0)
		return -1;

	return io_sscan(&m->io, data, bytes, NULL, smps, cnt);
}

int nanomsg_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	size_t wbytes;

	char data[NANOMSG_MAX_PACKET_LEN];

	ret = io_sprint(&m->io, data, sizeof(data), &wbytes, smps, cnt);
	if (ret <= 0)
		return -1;

	ret = nn_send(m->out.socket, data, wbytes, 0);
	if (ret < 0)
		return ret;

	return cnt;
}

int nanomsg_fd(struct node *n)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	int fd;
	size_t len = sizeof(fd);

	ret = nn_getsockopt(m->in.socket, NN_SOL_SOCKET, NN_RCVFD, &fd, &len);
	if (ret)
		return ret;

	return fd;
}

static struct plugin p = {
	.name		= "nanomsg",
	.description	= "scalability protocols library (libnanomsg)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct nanomsg),
		.type.stop	= nanomsg_type_stop,
		.reverse	= nanomsg_reverse,
		.parse		= nanomsg_parse,
		.print		= nanomsg_print,
		.start		= nanomsg_start,
		.stop		= nanomsg_stop,
		.read		= nanomsg_read,
		.write		= nanomsg_write,
		.fd		= nanomsg_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
