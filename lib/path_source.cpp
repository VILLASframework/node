/** Path source
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

#include <fmt/format.h>

#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/node.hpp>
#include <villas/path.hpp>
#include <villas/exceptions.hpp>
#include <villas/hook_list.hpp>

#include <villas/nodes/loopback_internal.hpp>

#include <villas/path_destination.hpp>
#include <villas/path_source.hpp>

using namespace villas;
using namespace villas::node;

PathSource::PathSource(Path *p, Node *n) :
	node(n),
	path(p),
	masked(false)
{
	int ret;

	int pool_size = MAX(DEFAULT_QUEUE_LENGTH, 20 * node->in.vectorize);
	ret = pool_init(&pool, pool_size, SAMPLE_LENGTH(node->getInputSignalsMaxCount()), node->getMemoryType());
	if (ret)
		throw RuntimeError("Failed to initialize pool");
}

PathSource::~PathSource()
{
	int ret __attribute__((unused));

	ret = pool_destroy(&pool);
}

int PathSource::read(int i)
{
	int ret, recv, tomux, allocated, cnt, toenqueue, enqueued = 0;

	cnt = node->in.vectorize;

	struct Sample *read_smps[cnt];
	struct Sample *muxed_smps[cnt];
	struct Sample **tomux_smps;

	/* Fill smps[] free sample blocks from the pool */
	allocated = sample_alloc_many(&pool, read_smps, cnt);
	if (allocated != cnt)
		path->logger->warn("Pool underrun for path source {}", *node);

	/* Read ready samples and store them to blocks pointed by smps[] */
	recv = node->read(read_smps, allocated);
	if (recv == 0) {
		enqueued = 0;
		goto out2;
	}
	else if (recv < 0) {
		if (node->getState() == State::STOPPING) {
			path->state = State::STOPPING;

			enqueued = -1;
			goto out2;
		}
		else {
			path->logger->error("Failed to read samples from node {}", *node);
			enqueued = 0;
			goto out2;
		}
	}
	else if (recv < allocated)
		path->logger->warn("Partial read for path {}: read={}, expected={}", *path, recv, allocated);

	/* Let the master path sources forward received samples to their secondaries */
	writeToSecondaries(read_smps, recv);

	if (path->mode == Path::Mode::ANY) { /* Mux all samples */
		tomux_smps = read_smps;
		tomux = recv;
	}
	else { /* Mux only last sample and discard others */
		tomux_smps = read_smps + recv - 1;
		tomux = 1;
	}

	for (int i = 0; i < tomux; i++) {
		muxed_smps[i] = i == 0
			? sample_clone(path->last_sample)
			: sample_clone(muxed_smps[i-1]);
		if (!muxed_smps[i]) {
			path->logger->error("Pool underrun in path {}", *path);
			return -1;
		}

		if (path->original_sequence_no) {
			muxed_smps[i]->sequence = tomux_smps[i]->sequence;
			muxed_smps[i]->flags |= tomux_smps[i]->flags & (int) SampleFlags::HAS_SEQUENCE;
		}
		else {
			muxed_smps[i]->sequence = path->last_sequence++;
			muxed_smps[i]->flags |= (int) SampleFlags::HAS_SEQUENCE;
		}

		/* We reset the sample length after each restart of the simulation.
		 * This is necessary for the test_rtt node to work properly.
		 */
		if (tomux_smps[i]->flags & (int) SampleFlags::IS_FIRST)
			muxed_smps[i]->length = 0;

		muxed_smps[i]->ts = tomux_smps[i]->ts;
		muxed_smps[i]->flags |= tomux_smps[i]->flags & (int) SampleFlags::HAS_TS;

		ret = mappings.remap(muxed_smps[i], tomux_smps[i]);
		if (ret)
			return ret;

		if (muxed_smps[i]->length > 0)
			muxed_smps[i]->flags |= (int) SampleFlags::HAS_DATA;
	}

	sample_copy(path->last_sample, muxed_smps[tomux-1]);

#ifdef WITH_HOOKS
	toenqueue = path->hooks.process(muxed_smps, tomux);
	if (toenqueue == -1) {
		path->logger->error("An error occured during hook processing. Skipping sample");

	}
	else if (toenqueue != tomux) {
		int skipped = tomux - toenqueue;

		path->logger->debug("Hooks skipped {} out of {} samples for path {}", skipped, tomux, *path);
	}
#else
	toenqueue = tomux;
#endif

	path->received.set(i);

	path->logger->debug("received=0b{:b}, mask=0b{:b}", path->received.to_ullong(), path->mask.to_ullong());

	if (path->mask.test(i)) {
		/* Enqueue always */
		if (path->mode == Path::Mode::ANY) {
			enqueued = toenqueue;
			PathDestination::enqueueAll(path, muxed_smps, toenqueue);
		}
		/* Enqueue only if received == mask bitset */
		else if (path->mode == Path::Mode::ALL) {
			if (path->mask == path->received) {
				PathDestination::enqueueAll(path, muxed_smps, toenqueue);

				path->received.reset();

				enqueued = toenqueue;
			}
			else
				enqueued = 0;
		}
	}
	else
		enqueued = 0;

	sample_decref_many(muxed_smps, tomux);
out2:	sample_decref_many(read_smps, recv);

	return enqueued;
}

void PathSource::check()
{
	if (!node->isEnabled())
		throw RuntimeError("Source {} is not enabled", *node);

	if (!(node->getFactory()->getFlags() & (int) NodeFactory::Flags::SUPPORTS_READ))
		throw RuntimeError("Node {} is not supported as a source for a path", *node);
}

MasterPathSource::MasterPathSource(Path *p, Node *n) :
	PathSource(p, n)
{ }

void MasterPathSource::writeToSecondaries(struct Sample *smps[], unsigned cnt)
{
	for (auto sps : secondaries) {
		int sent = sps->getNode()->write(smps, cnt);
		if (sent < 0)
			throw RuntimeError("Failed to write secondary path source {} of path {}", *sps->getNode(), *path);
		else if ((unsigned) sent < cnt)
			path->logger->warn("Partial write to secondary path source {} of path {}", *sps->getNode(), *path);
	}
}

SecondaryPathSource::SecondaryPathSource(Path *p, Node *n, NodeList &nodes, PathSource::Ptr m) :
	PathSource(p, n),
	master(m)
{
	auto mps = std::dynamic_pointer_cast<MasterPathSource>(m);

	node = new InternalLoopbackNode(n, mps->getSecondaries().size(), mps->getPath()->queuelen);
	if (!node)
		throw RuntimeError("Failed to create internal loopback");

	/* Register new loopback node in node list of super node */
	nodes.push_back(node);
}
