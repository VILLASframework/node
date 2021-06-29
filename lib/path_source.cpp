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

static void path_source_forward(struct vpath_source *ps, struct sample *smps[], unsigned cnt)
{
	for (size_t i = 0; i < vlist_length(&ps->secondaries); i++) {
		auto *sps = (struct vpath_source *) vlist_at(&ps->secondaries, i);

		int sent;

		sent = node_write(sps->node, smps, cnt);
		if (sent < 0)
			continue;

		if ((unsigned) sent < cnt)
			ps->logger->warn("Partial write to secondary path source {} of path {}", node_name(sps->node), path_name(ps->path));
	}
}

static int path_source_mux(struct vpath_source *ps, struct sample *out_smps[], struct sample *in_smps[], unsigned cnt)
{
	/* Mux only last sample and discard others */
	if (ps->path->mode == PathMode::ALL) {
		in_smps += cnt - 1;
		cnt = 1;
	}

	struct sample *last_sample;
	unsigned i;
	for (i = 0, last_sample = ps->path->last_sample;
	     i < cnt;
	     i++,   last_sample = out_smps[i - 1]) {

		out_smps[i] = sample_clone(last_sample);

		/* Seqeuence number */
		out_smps[i]->sequence = ps->path->original_sequence_no
			? in_smps[i]->sequence
			: ps->path->last_sequence_no++;
		out_smps[i]->flags |= (int) SampleFlags::HAS_SEQUENCE;

		/* Timestamp */
		out_smps[i]->ts = in_smps[i]->ts;
		out_smps[i]->flags |= in_smps[i]->flags & (int) SampleFlags::HAS_TS;

		mapping_list_remap(&ps->mappings, out_smps[i], in_smps[i]);

		if (out_smps[i]->length > 0)
			out_smps[i]->flags |= (int) SampleFlags::HAS_DATA;
	}

	/* Update last processed sample of path */
	sample_decref(ps->path->last_sample);
	sample_incref(last_sample);

	ps->path->last_sample = last_sample;

	return cnt;
}

int path_source_init_primary(struct vpath_source *ps, struct vpath *p, struct vnode *n)
{
	int ret;

	new (&ps->logger) Logger(logging.get("path:in"));

	ps->index = -1;
	ps->node = n;
	ps->path = p;
	ps->masked = false;
	ps->type = PathSourceType::PRIMARY;

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

	ret = path_source_init_primary(ps, p, n);
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

int path_source_read(struct vpath_source *ps)
{
	int cnt = ps->node->in.vectorize;

	int alloc_cnt;	/**< Number of samples allocated from pool */
	int read_cnt;	/**< Number of samples read from source node */
	int mux_cnt;	/**< Number of samples which have been muxed */
	int enq_cnt;	/**< Number of samples to be enqueued to destinations */

	struct sample *read_smps[cnt];
	struct sample *mux_smps_buf[cnt];
	struct sample **mux_smps;

	/* Fill read_smps[] free sample blocks from the pool */
	alloc_cnt = sample_alloc_many(&ps->pool, read_smps, cnt);
	if (alloc_cnt != cnt)
		ps->logger->warn("Pool underrun for path source {}", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by read_smps[] */
	read_cnt = node_read(ps->node, read_smps, alloc_cnt);
	if (read_cnt <= 0) {
		if (read_cnt < 0) {
			if (ps->node->state == State::STOPPING)
				ps->path->state = State::STOPPING;
			else
				ps->logger->error("Failed to read samples from node {}", node_name(ps->node));
		}

		goto out;
	}
	else if (read_cnt < alloc_cnt)
		ps->logger->warn("Partial read for path {}: read={}, expected={}", path_name(ps->path), read_cnt, alloc_cnt);

	/* Forward samples to secondary path sources */
	path_source_forward(ps, read_smps, read_cnt);

	/* Muxing */
	if (path_is_muxed(ps->path)) {
		mux_smps = mux_smps_buf;
		mux_cnt = path_source_mux(ps, mux_smps, read_smps, read_cnt);
	}
	else {
		mux_smps = read_smps;
		mux_cnt = read_cnt;
	}

#ifdef WITH_HOOKS
	enq_cnt = hook_list_process(&ps->path->hooks, mux_smps, mux_cnt);
	if (enq_cnt < 0) {
		ps->logger->error("An error occured during hook processing. Skipping sample");
		return -1;
	}
	else if (enq_cnt != mux_cnt) {
		int skipped = mux_cnt - enq_cnt;

		ps->logger->debug("Hooks skipped {} out of {} samples for path {}", skipped, mux_cnt, path_name(ps->path));
	}
#else
	enq_cnt = mux_cnt;
#endif

	ps->path->received.set(ps->index);
	ps->logger->debug("Path {} received={}", path_name(ps->path), ps->path->received.to_ullong());

	if (ps->path->mask.test(ps->index)) {
		/* Check if we received an update from all nodes */
		if ((ps->path->mode == PathMode::ANY) ||
		    (ps->path->mode == PathMode::ALL && ps->path->mask == ps->path->received)) {
			path_destination_enqueue_all(ps->path, mux_smps, enq_cnt);

			/* Reset mask of updated nodes */
			ps->path->received.reset();
		}
	}

	if (path_is_muxed(ps->path))
		sample_decref_many(mux_smps, mux_cnt);

out:	sample_decref_many(read_smps, alloc_cnt);

	return enq_cnt;
}

void path_source_check(struct vpath_source *ps)
{
	if (!node_is_enabled(ps->node))
		throw RuntimeError("Source {} is not enabled", node_name(ps->node));

	if (!node_type(ps->node)->read)
		throw RuntimeError("Node {} is not supported as a source for a path", node_name(ps->node));
}
