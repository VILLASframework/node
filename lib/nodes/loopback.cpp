/** Node-type for loopback connections.
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

#include <cstring>

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/node/config.h>
#include <villas/nodes/loopback.hpp>
#include <villas/memory.h>

using namespace villas::utils;

int loopback_parse(struct node *n, json_t *cfg)
{
	struct loopback *l = (struct loopback *) n->_vd;
	const char *mode_str = nullptr;

	json_error_t err;
	int ret;

	/* Default values */
	l->mode = QueueSignalledMode::AUTO;
	l->queuelen = DEFAULT_QUEUE_LENGTH;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: s }",
		"queuelen", &l->queuelen,
		"mode", &mode_str
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (mode_str) {
		if (!strcmp(mode_str, "auto"))
			l->mode = QueueSignalledMode::AUTO;
#ifdef HAVE_EVENTFD
		else if (!strcmp(mode_str, "eventfd"))
			l->mode = QueueSignalledMode::EVENTFD;
#endif
		else if (!strcmp(mode_str, "pthread"))
			l->mode = QueueSignalledMode::PTHREAD;
		else if (!strcmp(mode_str, "polling"))
			l->mode = QueueSignalledMode::POLLING;
#ifdef __APPLE__
		else if (!strcmp(mode_str, "pipe"))
			l->mode = QueueSignalledMode::PIPE;
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

	ret = pool_init(&l->pool, l->queuelen, SAMPLE_LENGTH(len));
	if (ret)
		return ret;

	return queue_signalled_init(&l->queue, l->queuelen, memory_default, l->mode);
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

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name			= "loopback";
	p.description		= "Loopback to connect multiple paths";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.flags		= 0;
	p.node.size		= sizeof(struct loopback);
	p.node.parse		= loopback_parse;
	p.node.print		= loopback_print;
	p.node.start		= loopback_start;
	p.node.stop		= loopback_stop;
	p.node.read		= loopback_read;
	p.node.write		= loopback_write;
	p.node.poll_fds		= loopback_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
