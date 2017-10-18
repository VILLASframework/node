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

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <string.h>

#include "plugin.h"
#include "nodes/nanomsg.h"
#include "utils.h"
#include "io_format.h"

int nanomsg_reverse(struct node *n)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	if (list_length(&m->publisher.endpoints)  != 1 ||
	    list_length(&m->subscriber.endpoints) != 1)
		return -1;

	char *subscriber = list_first(&m->subscriber.endpoints);
	char *publisher = list_first(&m->publisher.endpoints);

	list_set(&m->subscriber.endpoints, 0, publisher);
	list_set(&m->publisher.endpoints, 0, subscriber);

	return 0;
}

static int nanomsg_parse_endpoints(struct list *l, json_t *cfg)
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

int nanomsg_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	const char *format = "villas-human";

	json_error_t err;

	json_t *json_pub = NULL;
	json_t *json_sub = NULL;

	list_init(&m->publisher.endpoints);
	list_init(&m->subscriber.endpoints);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o, s?: o, s?: s }",
		"publish", &json_pub,
		"subscribe", &json_sub,
		"format", &format
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (json_pub) {
		ret = nanomsg_parse_endpoints(&m->publisher.endpoints, json_pub);
		if (ret < 0)
			error("Invalid type for 'publish' setting of node %s", node_name(n));
	}

	if (json_sub) {
		ret = nanomsg_parse_endpoints(&m->subscriber.endpoints, json_sub);
		if (ret < 0)
			error("Invalid type for 'subscribe' setting of node %s", node_name(n));
	}

	m->format = io_format_lookup(format);
	if (!m->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	return 0;
}

char * nanomsg_print(struct node *n)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	char *buf = NULL;

	strcatf(&buf, "format=%s, subscribe=[ ", plugin_name(m->format));

	for (size_t i = 0; i < list_length(&m->subscriber.endpoints); i++) {
		char *ep = (char *) list_at(&m->subscriber.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "], publish=[ ");

	for (size_t i = 0; i < list_length(&m->publisher.endpoints); i++) {
		char *ep = (char *) list_at(&m->publisher.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "]");

	return buf;
}

int nanomsg_start(struct node *n)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	ret = m->subscriber.socket = nn_socket(AF_SP, NN_SUB);
	if (ret < 0)
		return ret;

	ret = m->publisher.socket = nn_socket(AF_SP, NN_PUB);
	if (ret < 0)
		return ret;

	/* Subscribe to all topics */
	ret = nn_setsockopt(ret = m->subscriber.socket, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);
	if (ret < 0)
		return ret;

	/* Bind publisher to socket */
	for (size_t i = 0; i < list_length(&m->publisher.endpoints); i++) {
		char *ep = (char *) list_at(&m->publisher.endpoints, i);

		ret = nn_bind(m->publisher.socket, ep);
		if (ret < 0) {
			info("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}

	/* Sonnect subscribers socket */
	for (size_t i = 0; i < list_length(&m->subscriber.endpoints); i++) {
		char *ep = (char *) list_at(&m->subscriber.endpoints, i);

		ret = nn_connect(m->subscriber.socket, ep);
		if (ret < 0) {
			info("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}

	return 0;
}

int nanomsg_stop(struct node *n)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	ret = nn_close(m->subscriber.socket);
	if (ret < 0)
		return ret;

	ret = nn_close(m->publisher.socket);
	if (ret < 0)
		return ret;

	return 0;
}

int nanomsg_deinit()
{
	nn_term();

	return 0;
}

int nanomsg_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;
	int bytes;
	char data[NANOMSG_MAX_PACKET_LEN];

	/* Receive payload */
	bytes = nn_recv(m->subscriber.socket, data, sizeof(data), 0);
	if (bytes < 0)
		return -1;

	return io_format_sscan(m->format, data, bytes, NULL, smps, cnt, 0);
}

int nanomsg_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	size_t wbytes;

	char data[NANOMSG_MAX_PACKET_LEN];

	ret = io_format_sprint(m->format, data, sizeof(data), &wbytes, smps, cnt, SAMPLE_HAS_ALL);
	if (ret <= 0)
		return -1;

	ret = nn_send(m->publisher.socket, data, wbytes, 0);
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

	ret = nn_getsockopt(m->subscriber.socket, NN_SOL_SOCKET, NN_RCVFD, &fd, &len);
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
		.reverse	= nanomsg_reverse,
		.parse		= nanomsg_parse,
		.print		= nanomsg_print,
		.start		= nanomsg_start,
		.stop		= nanomsg_stop,
		.deinit		= nanomsg_deinit,
		.read		= nanomsg_read,
		.write		= nanomsg_write,
		.fd		= nanomsg_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
