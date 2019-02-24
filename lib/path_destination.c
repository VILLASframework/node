/** Path destination
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

#include <villas/utils.h>
#include <villas/memory.h>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/path_destination.h>

int path_destination_init(struct path_destination *pd, int queuelen)
{
	int ret;

	ret = queue_init(&pd->queue, queuelen, &memory_hugepage);
	if (ret)
		return ret;

	return 0;
}

int path_destination_destroy(struct path_destination *pd)
{
	int ret;

	ret = queue_destroy(&pd->queue);
	if (ret)
		return ret;

	return 0;
}

void path_destination_enqueue(struct path *p, struct sample *smps[], unsigned cnt)
{
	unsigned enqueued, cloned;

	struct sample *clones[cnt];

	cloned = sample_clone_many(clones, smps, cnt);
	if (cloned < cnt)
		warning("Pool underrun in path %s", path_name(p));

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

		enqueued = queue_push_many(&pd->queue, (void **) clones, cloned);
		if (enqueued != cnt)
			warning("Queue overrun for path %s", path_name(p));

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_incref_many(clones, cloned);

		debug(LOG_PATH | 15, "Enqueued %u samples to destination %s of path %s", enqueued, node_name(pd->node), path_name(p));
	}

	sample_decref_many(clones, cloned);
}

void path_destination_write(struct path_destination *pd, struct path *p)
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
			debug(LOG_PATH | 5, "Queue underrun for path %s: allocated=%u expected=%u", path_name(p), allocated, cnt);

		debug(LOG_PATH | 15, "Dequeued %u samples from queue of node %s which is part of path %s", allocated, node_name(pd->node), path_name(p));

		release = allocated;

		sent = node_write(pd->node, smps, allocated, &release);
		if (sent < 0)
			error("Failed to sent %u samples to node %s: reason=%d", cnt, node_name(pd->node), sent);
		else if (sent < allocated)
			warning("Partial write to node %s: written=%d, expected=%d", node_name(pd->node), sent, allocated);

		released = sample_decref_many(smps, release);

		debug(LOG_PATH | 15, "Released %d samples back to memory pool", released);
	}
}
