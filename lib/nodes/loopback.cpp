/** Node-type for loopback connections.
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

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/node/config.h>
#include <villas/nodes/loopback.hpp>
#include <villas/memory.h>

int loopback_parse(struct node *n, json_t *cfg)
{
	struct loopback *l = (struct loopback *) n->_vd;
	const char *mode_str = nullptr;

	json_error_t err;
	int ret;

	/* Default values */
	l->queueflags = QUEUE_SIGNALLED_AUTO;
	l->queuelen = DEFAULT_QUEUE_LENGTH;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: s }",
		"queuelen", &l->queuelen,
		"mode", &mode_str
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (mode_str) {
		if (!strcmp(mode_str, "auto"))
			l->queueflags = QUEUE_SIGNALLED_AUTO;
#ifdef HAVE_EVENTFD
		else if (!strcmp(mode_str, "eventfd"))
			l->queueflags = QUEUE_SIGNALLED_EVENTFD;
#endif
		else if (!strcmp(mode_str, "pthread"))
			l->queueflags = QUEUE_SIGNALLED_PTHREAD;
		else if (!strcmp(mode_str, "polling"))
			l->queueflags = QUEUE_SIGNALLED_POLLING;
#ifdef __APPLE__
		else if (!strcmp(mode_str, "pipe"))
			l->queueflags = QUEUE_SIGNALLED_PIPE;
#endif /* __APPLE__ */
		else
			error("Unknown mode '%s' in node %s", mode_str, node_name(n));
	}

	return 0;
}

int loopback_start(struct node *n)
{
	int ret;
	struct loopback *l = (struct loopback *) n->_vd;

	int len = MAX(
		vlist_length(&n->in.signals),
		vlist_length(&n->out.signals)
	);

	ret = pool_init(&l->pool, l->queuelen, SAMPLE_LENGTH(len), &memory_hugepage);
	if (ret)
		return ret;

	return queue_signalled_init(&l->queue, l->queuelen, &memory_hugepage, l->queueflags);
}

int loopback_stop(struct node *n)
{
	int ret;
	struct loopback *l= (struct loopback *) n->_vd;

	ret = pool_destroy(&l->pool);
	if (ret)
		return ret;

	return queue_signalled_destroy(&l->queue);
}

int loopback_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct loopback *l = (struct loopback *) n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&l->queue, (void **) cpys, cnt);

	for (int i = 0; i < avail; i++) {
		sample_copy(smps[i], cpys[i]);
		sample_decref(cpys[i]);
	}

	return avail;
}

int loopback_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct loopback *l = (struct loopback *) n->_vd;
	struct sample *copies[cnt];

	unsigned copied;

	copied = sample_alloc_many(&l->pool, copies, cnt);
	if (copied < cnt)
		warning("Pool underrun for node %s", node_name(n));

	sample_copy_many(copies, smps, copied);

	return queue_signalled_push_many(&l->queue, (void **) copies, copied);
}

char * loopback_print(struct node *n)
{
	struct loopback *l = (struct loopback *) n->_vd;
	char *buf = nullptr;

	strcatf(&buf, "queuelen=%d", l->queuelen);

	return buf;
}

int loopback_poll_fds(struct node *n, int fds[])
{
	struct loopback *l = (struct loopback *) n->_vd;

	fds[0] = queue_signalled_fd(&l->queue);

	return 1;
}

static struct plugin p = {
	.name = "loopback",
	.description = "Loopback to connect multiple paths",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.flags		= NODE_TYPE_PROVIDES_SIGNALS,
		.size		= sizeof(struct loopback),
		.parse		= loopback_parse,
		.print		= loopback_print,
		.start		= loopback_start,
		.stop		= loopback_stop,
		.read		= loopback_read,
		.write		= loopback_write,
		.poll_fds	= loopback_poll_fds
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
