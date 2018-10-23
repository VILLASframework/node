/** Node type: Ethercat
 *
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <villas/super_node.h>
#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/buffer.h>
#include <villas/plugin.h>
#include <villas/nodes/ethercat.h>
#include <villas/format_type.h>
#include <villas/formats/msg_format.h>

/* Private static storage */

/* Forward declarations */
static struct plugin p;


int ethercat_type_start(struct super_node *sn)
{
    /* TODO: type start ethercat */

	return 0;
}

int ethercat_start(struct node *n)
{
	int ret;
	struct ethercat *w = (struct ethercat *) n->_vd;

	ret = pool_init(&w->pool, DEFAULT_ETHERCAT_QUEUE_LENGTH, SAMPLE_LENGTH(DEFAULT_ETHERCAT_SAMPLE_LENGTH), &memory_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_ETHERCAT_QUEUE_LENGTH, &memory_hugepage, 0);
	if (ret)
		return ret;

    /* TODO: start ethercat connection */

	return 0;
}

int ethercat_stop(struct node *n)
{
	int ret;
	struct ethercat *w = (struct ethercat *) n->_vd;

    /* TODO: stop ethercat connection */

	ret = queue_signalled_destroy(&w->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	return 0;
}

int ethercat_destroy(struct node *n)
{
	struct websocket *w = (struct websocket *) n->_vd;
	int ret;

    /* TODO: destroy ethercat connection */

	if (ret)
		return ret;

	return 0;
}

int ethercat_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct ethercat *w = (struct ethercat *) n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
	if (avail < 0)
		return avail;

	sample_copy_many(smps, cpys, avail);
	sample_decref_many(cpys, avail);

	return avail;
}

int ethercat_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct ethercat *w = (struct ethercat *) n->_vd;
	struct sample *cpys[cnt];

	/* Make copies of all samples */
	avail = sample_alloc_many(&w->pool, cpys, cnt);
	if (avail < cnt)
		warn("Pool underrun for node %s: avail=%u", node_name(n), avail);

	sample_copy_many(cpys, smps, avail);

    /* TODO: write samples to ethercat driver */

	sample_decref_many(cpys, avail);

	return cnt;
}

int ethercat_parse(struct node *n, json_t *cfg)
{
	struct ethercat *w = (struct ethercat *) n->_vd;
	int ret;

	size_t i;
	json_t *json_dests = NULL;
	json_t *json_dest;
	json_error_t err;

    /* TODO: parse json */

	return 0;
}

char * ethercat_print(struct node *n)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	char *buf = NULL;
    /* TODO: maybe use asprintf to build string? */

	return buf;
}

int ethercat_fd(struct node *n)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	return queue_signalled_fd(&w->queue);
}

static struct plugin p = {
	.name		= "ethercat",
	.description	= "Send and receive samples of an ethercat connection",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct ethercat),
		.type.start	= ethercat_type_start,
		.start		= ethercat_start,
		.stop		= ethercat_stop,
		.destroy	= ethercat_destroy,
		.read		= ethercat_read,
		.write		= ethercat_write,
		.print		= ethercat_print,
		.parse		= ethercat_parse,
		.fd		    = ethercat_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
