/** Node-type for loopback connections.
 *
 * @file
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

#include "node.h"
#include "plugin.h"
#include "config.h"
#include "nodes/loopback.h"
#include "memory.h"

int loopback_parse(struct node *n, json_t *cfg)
{
	struct loopback *l = n->_vd;

	json_error_t err;
	int ret;

	/* Default values */
	l->queuelen = DEFAULT_QUEUELEN;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i }",
		"queuelen", &l->queuelen
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

int loopback_open(struct node *n)
{
	int ret;
	struct loopback *l = n->_vd;

	ret = pool_init(&l->pool, l->queuelen, SAMPLE_LEN(n->samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	return queue_signalled_init(&l->queue, l->queuelen, &memtype_hugepage, QUEUE_SIGNALLED_EVENTFD);
}

int loopback_close(struct node *n)
{
	int ret;
	struct loopback *l= n->_vd;

	ret = pool_destroy(&l->pool);
	if (ret)
		return ret;

	return queue_signalled_destroy(&l->queue);
}

int loopback_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int avail;

	struct loopback *l = n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&l->queue, (void **) cpys, cnt);

	for (int i = 0; i < avail; i++) {
		sample_copy(smps[i], cpys[i]);
		sample_put(cpys[i]);
	}

	return avail;
}

int loopback_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int cloned;

	struct loopback *l = n->_vd;
	struct sample *clones[cnt];

	cloned = sample_clone_many(smps, clones, cnt);
	if (cloned < cnt)
		warn("Pool underrun for node %s", node_name(n));

	return queue_signalled_push_many(&l->queue, (void **) clones, cloned);
}

char * loopback_print(struct node *n)
{
	struct loopback *l = n->_vd;
	char *buf = NULL;

	strcatf(&buf, "queuelen=%d", l->queuelen);

	return buf;
};

int loopback_fd(struct node *n)
{
	struct loopback *l = n->_vd;
	
	return queue_signalled_fd(&l->queue);
}

static struct plugin p = {
	.name = "loopback",
	.description = "Loopback to connect multiple paths",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.size	= sizeof(struct loopback),
		.parse	= loopback_parse,
		.print	= loopback_print,
		.start	= loopback_open,
		.stop	= loopback_close,
		.read	= loopback_read,
		.write	= loopback_write,
		.fd	= loopback_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
