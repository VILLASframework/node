/** Path source
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

#include <villas/utils.hpp>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/hook_list.hpp>

#include <villas/path_destination.h>
#include <villas/path_source.h>

int path_source_init(struct path_source *ps)
{
	int ret;
	int pool_size = MAX(DEFAULT_QUEUE_LENGTH, ps->node->in.vectorize);

	if (ps->node->_vt->pool_size)
		pool_size = ps->node->_vt->pool_size;

	ret = pool_init(&ps->pool, pool_size, SAMPLE_LENGTH(vlist_length(&ps->node->in.signals)), node_memory_type(ps->node, &memory_hugepage));
	if (ret)
		return ret;

	return 0;
}

int path_source_destroy(struct path_source *ps)
{
	int ret;

	ret = pool_destroy(&ps->pool);
	if (ret)
		return ret;

	ret = vlist_destroy(&ps->mappings, nullptr, false);
	if (ret)
		return ret;

	return 0;
}

int path_source_read(struct path_source *ps, struct path *p, int i)
{
	int ret, recv, tomux, allocated, cnt, toenqueue, enqueued = 0;
	unsigned release;

	cnt = ps->node->in.vectorize;

	struct sample *read_smps[cnt];
	struct sample *muxed_smps[cnt];
	struct sample **tomux_smps;

	/* Fill smps[] free sample blocks from the pool */
	allocated = sample_alloc_many(&ps->pool, read_smps, cnt);
	if (allocated != cnt)
		p->logger->warn("Pool underrun for path source {}", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	release = allocated;

	recv = node_read(ps->node, read_smps, allocated, &release);
	if (recv == 0) {
		enqueued = 0;
		goto out2;
	}
	else if (recv < 0) {
		if (ps->node->state == STATE_STOPPING) {
			p->state = STATE_STOPPING;

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

	p->received.set(i);

	if (p->mode == PATH_MODE_ANY) { /* Mux all samples */
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

		if (p->original_sequence_no)
			muxed_smps[i]->sequence = tomux_smps[i]->sequence;
		else {
			muxed_smps[i]->sequence = p->last_sequence++;
			muxed_smps[i]->flags |= SAMPLE_HAS_SEQUENCE;
		}

		/* We reset the sample length after each restart of the simulation.
		 * This is necessary for the test_rtt node to work properly.
		 */
		if (tomux_smps[i]->flags & SAMPLE_IS_FIRST)
			muxed_smps[i]->length = 0;

		muxed_smps[i]->ts = tomux_smps[i]->ts;
		muxed_smps[i]->flags |= tomux_smps[i]->flags & (SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_TS_RECEIVED);

		ret = mapping_list_remap(&ps->mappings, muxed_smps[i], tomux_smps[i]);
		if (ret)
			return ret;
	}

	sample_copy(p->last_sample, muxed_smps[tomux-1]);

	p->logger->debug("Path {} received = {}", path_name(p), p->received.to_ullong());

#ifdef WITH_HOOKS
	toenqueue = hook_list_process(&p->hooks, muxed_smps, tomux);
	if (toenqueue != tomux) {
		int skipped = tomux - toenqueue;

		p->logger->debug("Hooks skipped {} out of {} samples for path {}", skipped, tomux, path_name(p));
	}
#else
	toenqueue = tomux;
#endif

	if (p->mask.test(i)) {
		/* Check if we received an update from all nodes */
		if ((p->mode == PATH_MODE_ANY) ||
		    (p->mode == PATH_MODE_ALL && p->mask == p->received)) {
			path_destination_enqueue(p, muxed_smps, toenqueue);

			/* Reset mask of updated nodes */
			p->received.reset();

			enqueued = toenqueue;
		}
	}

	sample_decref_many(muxed_smps, tomux);
out2:	sample_decref_many(read_smps, release);

	return enqueued;
}
