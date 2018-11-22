/** Node type: rtp
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

#include <inttypes.h>
#include <string.h>

#include <re/re_types.h>
#include <re/re_main.h>
#include <re/re_rtp.h>
#undef ALIGN_MASK

#include <villas/plugin.h>
#include <villas/nodes/rtp.h>
#include <villas/utils.h>
#include <villas/format_type.h>

int rtp_reverse(struct node *n)
{
	/* struct rtp *m = (struct rtp *) n->_vd; */

	/* TODO */

	return -1;
}

int rtp_parse(struct node *n, json_t *cfg)
{
	int ret = 0;
	struct rtp *sr = (struct rtp *) n->_vd;

	const char *local, *remote;
	const char *format = "villas.binary";

	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: { s: s }, s: { s: s } }",
		"format", &format,
		"out",
			"address", &remote,
		"in",
			"address", &local
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	/* Format */
	sr->format = format_type_lookup(format);
	if(!sr->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	ret = sa_decode(&sr->remote, remote, strlen(remote));
	if (ret) {
		error("Failed to resolve remote address '%s' of node %s: %s",
			remote, node_name(n), strerror(ret));
	}

	ret = sa_decode(&sr->local, local, strlen(local));
	if (ret) {
		error("Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), strerror(ret));
	}

	info("### MKL ### rtp_parse success\n");

	return ret;
}

char * rtp_print(struct node *n)
{
	/* struct rtp *m = (struct rtp *) n->_vd; */
	
	char *buf = NULL;
	

	/* TODO */

	return buf;
}

int rtp_start(struct node *n)
{
	/*
	int ret;
	struct rtp *m = (struct rtp *) n->_vd;
	*/

	/* TODO */

	return -1;
}

int rtp_stop(struct node *n)
{
	/*
	int ret;
	struct rtp *m = (struct rtp *) n->_vd;
	*/

	/* TODO */
	re_cancel();

	return -1;
}

int rtp_type_stop()
{
	/* TODO */

	return -1;
}

int rtp_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	/*
	struct rtp *m = (struct rtp *) n->_vd;
	int bytes;
	char data[RTP_MAX_PACKET_LEN];
	*/

    /* TODO */

	return -1;
}

int rtp_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	/* struct rtp *m = (struct rtp *) n->_vd; */

	/* size_t wbytes; */

	/* char data[RTP_MAX_PACKET_LEN]; */

	/* TODO */
	rtp_send(NULL, NULL, 0, 0, 0, 0, NULL);

	return cnt;
}

int rtp_fd(struct node *n)
{
	/* struct rtp *m = (struct rtp *) n->_vd; */

	int fd = -1;

	/* TODO */

	return fd;
}

static struct plugin p = {
	.name		= "rtp",
	.description	= "real-time transport protocol (libre)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct rtp),
		.type.stop	= rtp_type_stop,
		.reverse	= rtp_reverse,
		.parse		= rtp_parse,
		.print		= rtp_print,
		.start		= rtp_start,
		.stop		= rtp_stop,
		.read		= rtp_read,
		.write		= rtp_write,
		.fd		= rtp_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
