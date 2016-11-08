/** Internal hook functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include "hooks.h"
#include "timing.h"
#include "sample.h"
#include "path.h"
#include "utils.h"

REGISTER_HOOK("fix_ts", "Update timestamps of sample if not set", 0, 0, hook_fix_ts, HOOK_AUTO | HOOK_READ)
int hook_fix_ts(struct hook *h, int when, struct hook_info *j)
{
	struct timespec now = time_now();

	assert(j->smps);

	for (int i = 0; i < j->cnt; i++) {
		/* Check for missing receive timestamp
		 * Usually node_type::read() should update the receive timestamp.
		 * An example would be to use hardware timestamp capabilities of
		 * modern NICs.
		 */
		if ((j->smps[i]->ts.received.tv_sec ==  0 && j->smps[i]->ts.received.tv_nsec ==  0) ||
		    (j->smps[i]->ts.received.tv_sec == -1 && j->smps[i]->ts.received.tv_nsec == -1))
			j->smps[i]->ts.received = now;

		/* Check for missing origin timestamp */
		if ((j->smps[i]->ts.origin.tv_sec ==  0 && j->smps[i]->ts.origin.tv_nsec ==  0) ||
		    (j->smps[i]->ts.origin.tv_sec == -1 && j->smps[i]->ts.origin.tv_nsec == -1))
			j->smps[i]->ts.origin = now;
	}

	return j->cnt;
}

REGISTER_HOOK("restart", "Call restart hooks for current path", 1, 1, hook_restart, HOOK_AUTO | HOOK_READ)
int hook_restart(struct hook *h, int when, struct hook_info *j)
{
	assert(j->smps);
	assert(j->path);

	for (int i = 0; i < j->cnt; i++) {
		h->last = j->smps[i];
		
		if (h->prev) {
			if (h->last->sequence  == 0 &&
			    h->prev->sequence <= UINT32_MAX - 32) {
				warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
					path_name(j->path), h->prev->sequence, h->last->sequence);

				hook_run(j->path, &j->smps[i], j->cnt - i, HOOK_PATH_RESTART);
			}
		}
		
		h->prev = h->last;
	}

	return j->cnt;
}

REGISTER_HOOK("drop", "Drop messages with reordered sequence numbers", 3, 1, hook_drop, HOOK_AUTO | HOOK_READ)
int hook_drop(struct hook *h, int when, struct hook_info *j)
{
	int i, ok, dist;
	
	assert(j->smps);

	for (i = 0, ok = 0; i < j->cnt; i++) {
		h->last = j->smps[i];
		
		if (h->prev) {
			dist = h->last->sequence - (int32_t) h->prev->sequence;
			if (dist <= 0) {
				stats_update(j->path->stats, STATS_DROPPED, dist);
				warn("Dropped sample: dist = %d, i = %d", dist, i);
			}
			else {
				struct sample *tmp;
			
				tmp = j->smps[i];
				j->smps[i] = j->smps[ok];
				j->smps[ok++] = tmp;
			}
		
			/* To discard the first X samples in 'smps[]' we must
			 * shift them to the end of the 'smps[]' array.
			 * In case the hook returns a number 'ok' which is smaller than 'cnt',
			 * only the first 'ok' samples in 'smps[]' are accepted and further processed.
			 */
		}
		else {
			struct sample *tmp;
		
			tmp = j->smps[i];
			j->smps[i] = j->smps[ok];
			j->smps[ok++] = tmp;
		}

		h->prev = h->last;
	}

	return ok;
}
