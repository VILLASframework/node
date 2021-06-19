/** Path destination
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

#include <villas/utils.hpp>
#include <villas/memory.h>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/exceptions.hpp>
#include <villas/path_destination.h>

using namespace villas;

int path_destination_init(struct vpath_destination *pd, struct vpath *p, struct vnode *n)
{
	new (&pd->logger) Logger(logging.get("path:out"));

	pd->node = n;
	pd->path = p;

	vlist_push(&n->destinations, pd);

	return 0;
}

int path_destination_prepare(struct vpath_destination *pd, int queuelen)
{
	int ret;

	ret = queue_init(&pd->queue, queuelen);
	if (ret)
		return ret;

	return 0;
}

int path_destination_destroy(struct vpath_destination *pd)
{
	int ret;

	ret = queue_destroy(&pd->queue);
	if (ret)
		return ret;

	return 0;
}

void path_destination_enqueue(struct vpath_destination *pd, struct sample * const smps[], unsigned cnt)
{
	unsigned enqueued;

	/* Increase reference counter of these samples as they are now also owned by the queue. */
	sample_incref_many(smps, cnt);

	enqueued = queue_push_many(&pd->queue, (void **) smps, cnt);
	if (enqueued != cnt)
		pd->logger->warn("Queue overrun for path {}", path_name(pd->path));

	pd->logger->debug("Enqueued {} samples to destination {} of path {}", enqueued, node_name(pd->node), path_name(pd->path));

}

void path_destination_write(struct vpath_destination *pd)
{
	int cnt = pd->node->out.vectorize;
	int sent;
	int pulled;

	struct sample *smps[cnt];

	/* As long as there are still samples in the queue */
	while (1) {
		pulled = queue_pull_many(&pd->queue, (void **) smps, cnt);
		if (pulled == 0)
			break;
		else if (pulled < cnt)
			pd->logger->debug("Queue underrun for path {}: pulled={} expected={}", path_name(pd->path), pulled, cnt);

		pd->logger->debug("Pulled {} samples from queue of node {} which is part of path {}", pulled, node_name(pd->node), path_name(pd->path));

		sent = node_write(pd->node, smps, pulled, true);
		if (sent < 0) {
			pd->logger->error("Failed to sent {} samples to node {}: reason={}", cnt, node_name(pd->node), sent);
			return;
		}
		else if (sent < pulled)
			pd->logger->debug("Partial write to node {}: written={}, expected={}", node_name(pd->node), sent, pulled);

		int released = sample_decref_many(smps, pulled);

		pd->logger->debug("Released {} samples back to memory pool", released);
	}
}

void path_destination_check(struct vpath_destination *pd)
{
	if (!node_is_enabled(pd->node))
		throw RuntimeError("Destination {} is not enabled", node_name(pd->node));

	if (!node_type(pd->node)->write)
		throw RuntimeError("Destiation node {} is not supported as a sink for path ", node_name(pd->node));
}

void path_destination_enqueue_all(struct vpath *p, struct sample * const smps[], unsigned cnt)
{
	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

		path_destination_enqueue(pd, smps, cnt);
	}
}

void path_destination_write_all(struct vpath *p)
{
	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

		path_destination_write(pd);
	}
}
