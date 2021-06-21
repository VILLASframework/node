/** Node-type for internal loopback_internal connections.
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

#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/nodes/loopback_internal.hpp>
#include <villas/exceptions.hpp>
#include <villas/memory.h>

using namespace villas;
using namespace villas::node;

static struct vnode_type p;

int loopback_internal_init(struct vnode *n)
{
	struct loopback_internal *l = (struct loopback_internal *) n->_vd;

	l->queuelen = DEFAULT_QUEUE_LENGTH;

	return 0;
}

int loopback_internal_prepare(struct vnode *n)
{
	int ret;
	struct loopback_internal *l = (struct loopback_internal *) n->_vd;

	ret = signal_list_copy(&n->in.signals, &l->source->in.signals);
	if (ret)
		return -1;

	return queue_signalled_init(&l->queue, l->queuelen, memory_default, QueueSignalledMode::EVENTFD);
}

int loopback_internal_destroy(struct vnode *n)
{
	struct loopback_internal *l= (struct loopback_internal *) n->_vd;

	return queue_signalled_destroy(&l->queue);
}

int loopback_internal_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	int avail;

	struct loopback_internal *l = (struct loopback_internal *) n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&l->queue, (void **) cpys, cnt);

	for (int i = 0; i < avail; i++) {
		sample_copy(smps[i], cpys[i]);
		sample_decref(cpys[i]);
	}

	return avail;
}

int loopback_internal_write(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	struct loopback_internal *l = (struct loopback_internal *) n->_vd;

	sample_incref_many(smps, cnt);

	return queue_signalled_push_many(&l->queue, (void **) smps, cnt);
}

int loopback_internal_poll_fds(struct vnode *n, int fds[])
{
	struct loopback_internal *l = (struct loopback_internal *) n->_vd;

	fds[0] = queue_signalled_fd(&l->queue);

	return 1;
}

struct vnode * loopback_internal_create(struct vnode *orig)
{
	int ret;
	struct vnode *n;
	struct loopback_internal *l;

	n = new struct vnode;
	if (!n)
		throw MemoryAllocationError();

	ret = node_init(n, &p);
	if (ret)
		return nullptr;

	l = (struct loopback_internal *) n->_vd;

	l->source = orig;

	asprintf(&n->name, "%s_lo%zu", node_name_short(orig), vlist_length(&orig->sources));

	return n;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "loopback_internal";
	p.description	= "internal loopback to connect multiple paths";
	p.vectorize	= 0;
	p.flags		= (int) NodeFlags::PROVIDES_SIGNALS | (int) NodeFlags::INTERNAL;
	p.size		= sizeof(struct loopback_internal);
	p.prepare	= loopback_internal_prepare;
	p.init		= loopback_internal_init;
	p.destroy	= loopback_internal_destroy;
	p.read		= loopback_internal_read;
	p.write		= loopback_internal_write;
	p.poll_fds	= loopback_internal_poll_fds;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
