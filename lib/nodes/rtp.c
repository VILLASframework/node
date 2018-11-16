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

/* TODO: individual includes instead of wrapper include */
#include <re/re.h>
#include <string.h>

#include <villas/plugin.h>
#include <villas/nodes/rtp.h>
#include <villas/utils.h>
#include <villas/format_type.h>

int rtp_reverse(struct node *n)
{
	struct rtp *m = (struct rtp *) n->_vd;

	/* TODO */

	return 0;
}

int rtp_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct rtp *m = (struct rtp *) n->_vd;

	const char *format = "villas.binary";

	json_error_t err;

	/* TODO ret = json_unpack_ex(...); */
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

char * rtp_print(struct node *n)
{
	struct rtp *m = (struct rtp *) n->_vd;

	char *buf = NULL;

	/* TODO */

	return buf;
}

int rtp_start(struct node *n)
{
	int ret;
	struct rtp *m = (struct rtp *) n->_vd;

	/* TODO */

	return 0;
}

int rtp_stop(struct node *n)
{
	int ret;
	struct rtp *m = (struct rtp *) n->_vd;

	/* TODO */

	return 0;
}

int rtp_type_stop()
{
	/* TODO */

	return -1;
}

int rtp_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct rtp *m = (struct rtp *) n->_vd;
	int bytes;
	char data[RTP_MAX_PACKET_LEN];

    /* TODO */

	return -1;
}

int rtp_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct rtp *m = (struct rtp *) n->_vd;

	size_t wbytes;

	char data[RTP_MAX_PACKET_LEN];

	/* TODO */

	return cnt;
}

int rtp_fd(struct node *n)
{
	int ret;
	struct rtp *m = (struct rtp *) n->_vd;

	int fd;
	size_t len = sizeof(fd);

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
