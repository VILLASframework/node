/** Path source
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

#include <fmt/format.h>

#include <villas/utils.hpp>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/exceptions.hpp>
#include <villas/hook_list.hpp>

#include <villas/nodes/loopback_internal.hpp>

#include <villas/path_destination.h>
#include <villas/path_source.h>

using namespace villas;

int path_source_init_master(struct vpath_source *ps, struct vnode *n)
{
	int ret;

	ps->node = n;
	ps->masked = false;
	ps->type = PathSourceType::MASTER;

	ret = vlist_init(&ps->mappings);
	if (ret)
		return ret;

	ret = vlist_init(&ps->secondaries);
	if (ret)
		return ret;

	int pool_size = MAX(DEFAULT_QUEUE_LENGTH, 40 * ps->node->in.vectorize);

	if (ps->node->_vt->pool_size)
		pool_size = ps->node->_vt->pool_size;

	ret = pool_init(&ps->pool, pool_size, SAMPLE_LENGTH(vlist_length(node_input_signals(ps->node))), node_memory_type(ps->node));
	if (ret)
		return ret;

	return 0;
}

int path_source_init_secondary(struct vpath_source *ps, struct vnode *n)
{
	int ret;
	struct vpath_source *mps;

	ret = path_source_init_master(ps, n);
	if (ret)
		return ret;

	ps->type = PathSourceType::SECONDARY;

	ps->node = loopback_internal_create(n);
	if (!ps->node)
		return -1;

	ret = node_check(ps->node);
	if (ret)
		return -1;

	ret = node_prepare(ps->node);
	if (ret)
		return -1;

	mps = (struct vpath_source *) vlist_at_safe(&n->sources, 0);
	if (!mps)
		return -1;

	vlist_push(&mps->secondaries, ps);

	return 0;
}

int path_source_destroy(struct vpath_source *ps)
{
	int ret;

	ret = pool_destroy(&ps->pool);
	if (ret)
		return ret;

	ret = vlist_destroy(&ps->mappings, nullptr, false);
	if (ret)
		return ret;

	ret = vlist_destroy(&ps->secondaries, nullptr, false);
	if (ret)
		return ret;

	return 0;
}

int path_source_read(struct vpath_source *ps, struct vpath *p, int i)
{
	int ret, recv, tomux, allocated, cnt, toenqueue, enqueued = 0;

	cnt = ps->node->in.vectorize;

	struct sample *read_smps[cnt];
	struct sample *muxed_smps[cnt];
	struct sample **tomux_smps;

	/* Fill smps[] free sample blocks from the pool */
	allocated = sample_alloc_many(&ps->pool, read_smps, cnt);
	if (allocated != cnt)
		p->logger->warn("Pool underrun for path source {}", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	recv = node_read(ps->node, read_smps, allocated);
	if (recv == 0) {
		enqueued = 0;
		goto out2;
	}
	else if (recv < 0) {
		if (ps->node->state == State::STOPPING) {
			p->state = State::STOPPING;

			enqueued = -1;
			goto out2;
		}
		else {
			p->logger->error("Failed to read samples from node {}", node_name(ps->node));
			goto out2;
		}
	}
	else if (recv < allocated)
		p->logger->warn("Partial read for path {}: read={}, expected={}", path_name(p), recv, allocated);

	/* Forward samples to secondary path sources */
	for (size_t i = 0; i < vlist_length(&ps->secondaries); i++) {
		auto *sps = (struct vpath_source *) vlist_at(&ps->secondaries, i);

		int sent;

		sent = node_write(sps->node, read_smps, recv);
		if (sent < recv)
			p->logger->warn("Partial write to secondary path source {} of path {}", node_name(sps->node), path_name(p));

		sample_incref_many(read_smps, recv);
	}

	p->received.set(i);

	if (p->mode == PathMode::ANY) { /* Mux all samples */
		tomux_smps = read_smps;
		tomux = recv;
	}
	else { /* Mux only last sample and discard others */
		tomux_smps = read_smps + recv - 1;
		tomux = 1;
	}

	for (int i = 0; i < tomux; i++) {
		muxed_smps[i] = i == 0
			? sample_clone(p->last_sample)
			: sample_clone(muxed_smps[i-1]);
		if (!muxed_smps[i]) {
			p->logger->error("Pool underrun in path {}", path_name(p));
			return -1;
		}

		if (p->original_sequence_no) {
			muxed_smps[i]->sequence = tomux_smps[i]->sequence;
			muxed_smps[i]->flags |= tomux_smps[i]->flags & (int) SampleFlags::HAS_SEQUENCE;
		}
		else {
			muxed_smps[i]->sequence = p->last_sequence++;
			muxed_smps[i]->flags |= (int) SampleFlags::HAS_SEQUENCE;
		}

		/* We reset the sample length after each restart of the simulation.
		 * This is necessary for the test_rtt node to work properly.
		 */
		if (tomux_smps[i]->flags & (int) SampleFlags::IS_FIRST)
			muxed_smps[i]->length = 0;

		muxed_smps[i]->ts = tomux_smps[i]->ts;
		muxed_smps[i]->flags |= tomux_smps[i]->flags & (int) SampleFlags::HAS_TS;

		ret = mapping_list_remap(&ps->mappings, muxed_smps[i], tomux_smps[i]);
		if (ret)
			return ret;

		if (muxed_smps[i]->length > 0)
			muxed_smps[i]->flags |= (int) SampleFlags::HAS_DATA;
	}

	sample_copy(p->last_sample, muxed_smps[tomux-1]);

	p->logger->debug("Path {} received = {}", path_name(p), p->received.to_ullong());

#ifdef WITH_HOOKS
	toenqueue = hook_list_process(&p->hooks, muxed_smps, tomux);
	if (toenqueue == -1) {
		p->logger->error("An error occured during hook processing. Skipping sample");

	}
	else if (toenqueue != tomux) {
		int skipped = tomux - toenqueue;

		p->logger->debug("Hooks skipped {} out of {} samples for path {}", skipped, tomux, path_name(p));
	}
#else
	toenqueue = tomux;
#endif

	if (p->mask.test(i)) {
		/* Check if we received an update from all nodes */
		if ((p->mode == PathMode::ANY) ||
		    (p->mode == PathMode::ALL && p->mask == p->received)) {
			path_destination_enqueue(p, muxed_smps, toenqueue);

			/* Reset mask of updated nodes */
			p->received.reset();

			enqueued = toenqueue;
		}
	}

	sample_decref_many(muxed_smps, tomux);
out2:	sample_decref_many(read_smps, recv);

	return enqueued;
}

void path_source_check(struct vpath_source *ps)
{
	if (!node_is_enabled(ps->node))
		throw RuntimeError("Source {} is not enabled", node_name(ps->node));

	if (!node_type(ps->node)->read)
		throw RuntimeError("Node {} is not supported as a source for a path", node_name(ps->node));
}
