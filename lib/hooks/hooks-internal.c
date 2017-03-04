/** Internal hook functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include "hooks.h"
#include "timing.h"
#include "sample.h"
#include "path.h"
#include "utils.h"

REGISTER_HOOK("fix_ts", "Update timestamps of sample if not set", 0, 0, hook_fix_ts, HOOK_INTERNAL | HOOK_READ)
int hook_fix_ts(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	struct timespec now = time_now();

	for (int i = 0; i < cnt; i++) {
		/* Check for missing receive timestamp
		 * Usually node_type::read() should update the receive timestamp.
		 * An example would be to use hardware timestamp capabilities of
		 * modern NICs.
		 */
		if ((smps[i]->ts.received.tv_sec ==  0 && smps[i]->ts.received.tv_nsec ==  0) ||
		    (smps[i]->ts.received.tv_sec == -1 && smps[i]->ts.received.tv_nsec == -1))
			smps[i]->ts.received = now;

		/* Check for missing origin timestamp */
		if ((smps[i]->ts.origin.tv_sec ==  0 && smps[i]->ts.origin.tv_nsec ==  0) ||
		    (smps[i]->ts.origin.tv_sec == -1 && smps[i]->ts.origin.tv_nsec == -1))
			smps[i]->ts.origin = now;
	}

	return cnt;
}

REGISTER_HOOK("restart", "Call restart hooks for current path", 1, 1, hook_restart, HOOK_INTERNAL | HOOK_READ)
int hook_restart(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	for (int i = 0; i < cnt; i++) {
		h->last = smps[i];
		
		if (h->prev) {
			if (h->last->sequence  == 0 &&
			    h->prev->sequence <= UINT32_MAX - 32) {
				warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
					path_name(p), h->prev->sequence, h->last->sequence);

				p->invalid  =
				p->skipped  =
				p->dropped  = 0;
			
				hook_run(p, &smps[i], cnt - i, HOOK_PATH_RESTART);
			}
		}
		
		h->prev = h->last;
	}

	return cnt;
}

REGISTER_HOOK("drop", "Drop messages with reordered sequence numbers", 3, 1, hook_drop, HOOK_INTERNAL | HOOK_READ)
int hook_drop(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	int i, ok, dist;

	for (i = 0, ok = 0; i < cnt; i++) {
		h->last = smps[i];
		
		if (h->prev) {
			dist = h->last->sequence - (int32_t) h->prev->sequence;
			if (dist <= 0) {
				p->dropped++;
				warn("Dropped sample: dist = %d, i = %d", dist, i);
			}
			else {
				struct sample *tmp;
			
				tmp = smps[i];
				smps[i] = smps[ok];
				smps[ok++] = tmp;
			}
		
			/* To discard the first X samples in 'smps[]' we must
			 * shift them to the end of the 'smps[]' array.
			 * In case the hook returns a number 'ok' which is smaller than 'cnt',
			 * only the first 'ok' samples in 'smps[]' are accepted and further processed.
			 */
		}
		else {
			struct sample *tmp;
		
			tmp = smps[i];
			smps[i] = smps[ok];
			smps[ok++] = tmp;
		}
		
		h->prev = h->last;
	}

	return ok;
}
