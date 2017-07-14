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

int loopback_parse(struct node *n, config_setting_t *cfg)
{
	struct loopback *l = n->_vd;

	if (!config_setting_lookup_int(cfg, "queuelen", &l->queuelen))
		l->queuelen = DEFAULT_QUEUELEN;

	return 0;
}

int loopback_open(struct node *n)
{
	struct loopback *l = n->_vd;

	return queue_signalled_init(&l->queue, l->queuelen, &memtype_hugepage);
}

int loopback_close(struct node *n)
{
	struct loopback *l= n->_vd;

	return queue_signalled_destroy(&l->queue);
}

int loopback_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct loopback *l = n->_vd;

	return queue_signalled_pull_many(&l->queue, (void **) smps, cnt);
}

int loopback_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct loopback *l = n->_vd;

	return queue_signalled_push_many(&l->queue, (void **) smps, cnt);
}

char * loopback_print(struct node *n)
{
	struct loopback *l = n->_vd;
	char *buf = NULL;

	strcatf(&buf, "queuelen=%d", l->queuelen);

	return buf;
};

static struct plugin p = {
	.name = "loopback",
	.description = "Loopback to connect multiple paths",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.size  = sizeof(struct loopback),
		.parse = loopback_parse,
		.print = loopback_print,
		.start = loopback_open,
		.stop  = loopback_close,
		.read  = loopback_read,
		.write = loopback_write
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
