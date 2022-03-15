/** Path destination
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/node/memory.hpp>
#include <villas/sample.hpp>
#include <villas/node.hpp>
#include <villas/path.hpp>
#include <villas/exceptions.hpp>
#include <villas/path_destination.hpp>

using namespace villas;
using namespace villas::node;

PathDestination::PathDestination(Path *p, Node *n) :
	node(n),
	path(p)
{
	queue.state = State::DESTROYED;
}

PathDestination::~PathDestination()
{
	int ret __attribute__((unused));

	ret = queue_destroy(&queue);
}

int PathDestination::prepare(int queuelen)
{
	int ret;

	ret = queue_init(&queue, queuelen);
	if (ret)
		return ret;

	return 0;
}

void PathDestination::enqueueAll(Path *p, const struct Sample * const smps[], unsigned cnt)
{
	unsigned enqueued, cloned;

	struct Sample *clones[cnt];

	cloned = sample_clone_many(clones, smps, cnt);
	if (cloned < cnt)
		p->logger->warn("Pool underrun in path {}", *p);

	for (auto pd : p->destinations) {
		enqueued = queue_push_many(&pd->queue, (void **) clones, cloned);
		if (enqueued != cnt)
			p->logger->warn("Queue overrun for path {}", *p);

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_incref_many(clones, cloned);

		p->logger->debug("Enqueued {} samples to destination {} of path {}", enqueued, *pd->node, *p);
	}

	sample_decref_many(clones, cloned);
}

void PathDestination::write()
{
	int cnt = node->out.vectorize;
	int sent;
	int allocated;

	struct Sample *smps[cnt];

	/* As long as there are still samples in the queue */
	while (true) {
		allocated = queue_pull_many(&queue, (void **) smps, cnt);
		if (allocated == 0)
			break;
		else if (allocated < cnt)
			path->logger->debug("Queue underrun for path {}: allocated={} expected={}", *path, allocated, cnt);

		path->logger->debug("Dequeued {} samples from queue of node {} which is part of path {}", allocated, *node, *path);

		sent = node->write(smps, allocated);
		if (sent < 0) {
			path->logger->error("Failed to sent {} samples to node {}: reason={}", cnt, *node, sent);
			return;
		}
		else if (sent < allocated)
			path->logger->debug("Partial write to node {}: written={}, expected={}", *node, sent, allocated);

		int released = sample_decref_many(smps, allocated);

		path->logger->debug("Released {} samples back to memory pool", released);
	}
}

void PathDestination::check()
{
	if (!node->isEnabled())
		throw RuntimeError("Destination {} is not enabled", *node);

	if (!(node->getFactory()->getFlags() & (int) NodeFactory::Flags::SUPPORTS_WRITE))
		throw RuntimeError("Destination node {} is not supported as a sink for path ", *node);
}
