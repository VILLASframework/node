/** Node type: nanomsg
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

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <cstring>

#include <villas/plugin.h>
#include <villas/nodes/nanomsg.hpp>
#include <villas/utils.hpp>
#include <villas/format_type.h>

using namespace villas::utils;

int nanomsg_reverse(struct vnode *n)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	if (vlist_length(&m->out.endpoints)  != 1 ||
	    vlist_length(&m->in.endpoints) != 1)
		return -1;

	char *subscriber = (char *) vlist_first(&m->in.endpoints);
	char *publisher = (char *) vlist_first(&m->out.endpoints);

	vlist_set(&m->in.endpoints, 0, publisher);
	vlist_set(&m->out.endpoints, 0, subscriber);

	return 0;
}

static int nanomsg_parse_endpoints(struct vlist *l, json_t *cfg)
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

				vlist_push(l, strdup(ep));
			}
			break;

		case JSON_STRING:
			ep = json_string_value(cfg);

			vlist_push(l, strdup(ep));
			break;

		default:
			return -1;
	}

	return 0;
}

int nanomsg_parse(struct vnode *n, json_t *cfg)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	const char *format = "villas.binary";

	json_error_t err;

	json_t *json_out_endpoints = nullptr;
	json_t *json_in_endpoints = nullptr;

	vlist_init(&m->out.endpoints);
	vlist_init(&m->in.endpoints);

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

char * nanomsg_print(struct vnode *n)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	char *buf = nullptr;

	strcatf(&buf, "format=%s, in.endpoints=[ ", format_type_name(m->format));

	for (size_t i = 0; i < vlist_length(&m->in.endpoints); i++) {
		char *ep = (char *) vlist_at(&m->in.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "], out.endpoints=[ ");

	for (size_t i = 0; i < vlist_length(&m->out.endpoints); i++) {
		char *ep = (char *) vlist_at(&m->out.endpoints, i);

		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, "]");

	return buf;
}

int nanomsg_start(struct vnode *n)
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	ret = io_init(&m->io, m->format, &n->in.signals, (int) SampleFlags::HAS_ALL & ~(int) SampleFlags::HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&m->io);
	if (ret)
		return ret;

	ret = m->in.socket = nn_socket(AF_SP, NN_SUB);
	if (ret < 0) {
		warning("Failed to create nanomsg socket: node=%s, error=%s", node_name(n), nn_strerror(errno));
		return ret;
	}

	ret = m->out.socket = nn_socket(AF_SP, NN_PUB);
	if (ret < 0) {
		warning("Failed to create nanomsg socket: node=%s, error=%s", node_name(n), nn_strerror(errno));
		return ret;
	}

	/* Subscribe to all topics */
	ret = nn_setsockopt(ret = m->in.socket, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);
	if (ret < 0)
		return ret;

	/* Bind publisher to socket */
	for (size_t i = 0; i < vlist_length(&m->out.endpoints); i++) {
		char *ep = (char *) vlist_at(&m->out.endpoints, i);

		ret = nn_bind(m->out.socket, ep);
		if (ret < 0) {
			warning("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}

	/* Connect subscribers socket */
	for (size_t i = 0; i < vlist_length(&m->in.endpoints); i++) {
		char *ep = (char *) vlist_at(&m->in.endpoints, i);

		ret = nn_connect(m->in.socket, ep);
		if (ret < 0) {
			warning("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}

	return 0;
}

int nanomsg_stop(struct vnode *n)
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

int nanomsg_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;
	int bytes;
	char data[NANOMSG_MAX_PACKET_LEN];

	/* Receive payload */
	bytes = nn_recv(m->in.socket, data, sizeof(data), 0);
	if (bytes < 0)
		return -1;

	return io_sscan(&m->io, data, bytes, nullptr, smps, cnt);
}

int nanomsg_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
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

int nanomsg_poll_fds(struct vnode *n, int fds[])
{
	int ret;
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	int fd;
	size_t len = sizeof(fd);

	ret = nn_getsockopt(m->in.socket, NN_SOL_SOCKET, NN_RCVFD, &fd, &len);
	if (ret)
		return ret;

	fds[0] = fd;

	return 1;
}

int nanomsg_netem_fds(struct vnode *n, int fds[])
{
	struct nanomsg *m = (struct nanomsg *) n->_vd;

	fds[0] = m->out.socket;

	return 1;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "nanomsg";
	p.description		= "scalability protocols library (libnanomsg)";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct nanomsg);
	p.node.type.stop	= nanomsg_type_stop;
	p.node.parse		= nanomsg_parse;
	p.node.print		= nanomsg_print;
	p.node.start		= nanomsg_start;
	p.node.stop		= nanomsg_stop;
	p.node.read		= nanomsg_read;
	p.node.write		= nanomsg_write;
	p.node.reverse		= nanomsg_reverse;
	p.node.poll_fds		= nanomsg_poll_fds;
	p.node.netem_fds	= nanomsg_netem_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
