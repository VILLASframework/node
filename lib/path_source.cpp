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

int path_source_init_master(struct vpath_source *ps, struct vpath *p, struct vnode *n)
{
	int ret;

	new (&pd->logger) Logger(logging.get("path:in"));

	ps->node = n;
	ps->path = p;
	ps->masked = false;
	ps->type = PathSourceType::MASTER;

	ret = vlist_init(&ps->mappings);
	if (ret)
		return ret;

	ret = vlist_init(&ps->secondaries);
	if (ret)
		return ret;

	int pool_size = MAX(DEFAULT_QUEUE_LENGTH, ps->node->in.vectorize);

	if (ps->node->_vt->pool_size)
		pool_size = ps->node->_vt->pool_size;

	ret = pool_init(&ps->pool, pool_size, SAMPLE_LENGTH(vlist_length(node_input_signals(ps->node))), node_memory_type(ps->node));
	if (ret)
		return ret;

	return 0;
}

int path_source_init_secondary(struct vpath_source *ps, struct vpath *p, struct vnode *n)
{
	int ret;
	struct vpath_source *mps;

	ret = path_source_init_master(ps, n);
	if (ret)
		return ret;

	ps->type = PathSourceType::SECONDARY;

	ps->path = p;
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

int path_source_read(struct vpath_source *ps, int i)
{
	int ret;
	int alloc_cnt
	int read_cnt;
	int mux_cnt;
	int allocated_cnt;
	int toenqueue_cnt;
	int enqueued_cnt = 0;

	struct sample *smps[cnt];
	struct sample *muxed_smps[cnt];
	struct sample **mux_smps;

	/* Fill smps[] free sample blocks from the pool */
	alloc_cnt = sample_alloc_many(&ps->pool, smps, ps->node->in.vectorize);
	if (smp_cnt != ps->node->in.vectorize)
		ps->logger->warn("Pool underrun for path source {}", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	read_cnt = node_read(ps->node, smps, alloc_cnt);
	if (read_cnt == 0)
		goto out2;
	else if (read_cnt < 0) {
		if (ps->node->state == State::STOPPING) {
			p->state = State::STOPPING;

			enqueued = -1;
			goto out2;
		}
		else {
			ps->logger->error("Failed to read samples from node {}", node_name(ps->node));
			goto out2;
		}
	}
	else if (read_cnt < allocated)
		ps->logger->warn("Partial read for path {}: read={}, expected={}", path_name(ps->path), recv, allocated);

	/* Forward samples to secondary path sources */
	path_source_forward(ps, read_smps, read_cnt);

	p->received.set(i);

	if (p->mode == PathMode::ANY) { /* Mux all samples */
		mux_smps = read_smps;
		mux_cnt = read_cnt;
	}
	else { /* Mux only last sample and discard others */
		mux_smps = read_smps + read_cnt - 1;
		mux_cnt = 1;
	}

	path_source_mux(ps, muxed_smps, mux_smps, mux_cnt);

	ps->logger->debug("Path {} received = {}", path_name(ps->path), p->received.to_ullong());

#ifdef WITH_HOOKS
	enqueue_cnt = hook_list_process(&p->hooks, muxed_smps, mux_cnt);
	if (enqueue_cnt == -1) {
		ps->logger->error("An error occured during hook processing. Skipping sample");

	}
	else if (enqueue_cnt != mux_cnt) {
		int skipped = mux_cnt - enqueue_cnt;

		ps->logger->debug("Hooks skipped {} out of {} samples for path {}", skipped, mux_cnt, path_name(ps->path));
	}
#else
	enqueue_cnt = mux_cnt;
#endif

	if (p->mask.test(i)) {
		/* Check if we received an update from all nodes */
		if ((p->mode == PathMode::ANY) ||
		    (p->mode == PathMode::ALL && p->mask == p->received)) {
			path_destination_enqueue_all(p, muxed_smps, toenqueue);

			/* Reset mask of updated nodes */
			p->received.reset();

			enqueued = toenqueue;
		}
	}

	sample_decref_many(muxed_smps, mux_cnt);
out2:	sample_decref_many(smps, alloc_cont);

	return enqueued;
}

void path_source_check(struct vpath_source *ps)
{
	if (!node_is_enabled(ps->node))
		throw RuntimeError("Source {} is not enabled", node_name(ps->node));

	if (!node_type(ps->node)->read)
		throw RuntimeError("Node {} is not supported as a source for a path", node_name(ps->node));
}

void path_source_forward(struct vpath_source *ps, const struct *smps[], unsigned cnt)
{
	for (size_t i = 0; i < vlist_length(&ps->secondaries); i++) {
		auto *sps = (struct vpath_source *) vlist_at(&ps->secondaries, i);

		int sent;

		sent = node_write(sps->node, smps, cnt);
		if (sent < cnt)
			ps->logger->warn("Partial write to secondary path source {} of path {}", node_name(sps->node), path_name(ps->path));

		sample_incref_many(smps, cnt);
	}
}


void path_source_mux(struct vpath_source *ps, struct *smps_out[], struct sample *smps_in[], unsigned cnt)
{
	for (int i = 0; i < cnt; i++) {
		smps_out[i] = i == 0
			? sample_clone(p->last_sample)
			: sample_clone(muxed_smps[i-1]);

		if (p->original_sequence_no) {
			smps_out[i]->sequence = smps_in[i]->sequence;
			smps_out[i]->flags |= smps_in[i]->flags & (int) SampleFlags::HAS_SEQUENCE;
		}
		else {
			smps_out[i]->sequence = p->last_sequence++;
			smps_out[i]->flags |= (int) SampleFlags::HAS_SEQUENCE;
		}

		/* We reset the sample length after each restart of the simulation.
		 * This is necessary for the test_rtt node to work properly.
		 */
		if (smps_in[i]->flags & (int) SampleFlags::IS_FIRST)
			smps_out[i]->length = 0;

		smps_out[i]->ts = smps_in[i]->ts;
		smps_out[i]->flags |= smps_in[i]->flags & (int) SampleFlags::HAS_TS;

		ret = mapping_list_remap(&ps->mappings, smps_out[i], smps_in[i]);
		if (ret)
			return ret;

		if (smps_out[i]->length > 0)
			smps_out[i]->flags |= (int) SampleFlags::HAS_DATA;
	}

	sample_copy(p->last_sample, smps_out[cnt - 1]);
}
