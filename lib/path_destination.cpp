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
#include <villas/path_destination.h>

int path_destination_init(struct vpath_destination *pd)
{
	pd->node = n;
	pd->node->output_path = p;
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

void path_destination_enqueue(struct vpath *p, struct sample *smps[], unsigned cnt)
{
	unsigned enqueued, cloned;

	struct sample *clones[cnt];

	cloned = sample_clone_many(clones, smps, cnt);
	if (cloned < cnt)
		p->logger->warn("Pool underrun in path {}", path_name(p));

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

		enqueued = queue_push_many(&pd->queue, (void **) clones, cloned);
		if (enqueued != cnt)
			p->logger->warn("Queue overrun for path {}", path_name(p));

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_incref_many(clones, cloned);

		p->logger->debug("Enqueued {} samples to destination {} of path {}", enqueued, node_name(pd->node), path_name(p));
	}

	sample_decref_many(clones, cloned);
}

void path_destination_write(struct vpath_destination *pd, struct vpath *p)
{
	int cnt = pd->node->out.vectorize;
	int sent;
	int released;
	int allocated;
	unsigned release;

	struct sample *smps[cnt];

	/* As long as there are still samples in the queue */
	while (1) {
		allocated = queue_pull_many(&pd->queue, (void **) smps, cnt);
		if (allocated == 0)
			break;
		else if (allocated < cnt)
			p->logger->debug("Queue underrun for path {}: allocated={} expected={}", path_name(p), allocated, cnt);

		p->logger->debug("Dequeued {} samples from queue of node {} which is part of path {}", allocated, node_name(pd->node), path_name(p));

		release = allocated;

		sent = node_write(pd->node, smps, allocated, &release);
		if (sent < 0) {
			p->logger->error("Failed to sent {} samples to node {}: reason={}", cnt, node_name(pd->node), sent);
			return;
		}
		else if (sent < allocated)
			p->logger->debug("Partial write to node {}: written={}, expected={}", node_name(pd->node), sent, allocated);

		released = sample_decref_many(smps, release);

		p->logger->debug("Released {} samples back to memory pool", released);
	}
}
