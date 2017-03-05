/** Drop hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"
#include "stats.h"
#include "path.h"

static int hook_drop(struct hook *h, int when, struct hook_info *j)
{
	int i, ok, dist;
	
	assert(j->smps);

	for (i = 0, ok = 0; i < j->cnt; i++) {
		h->last = j->smps[i];
		
		if (h->prev) {
			dist = h->last->sequence - (int32_t) h->prev->sequence;
			if (dist <= 0) {
				warn("Dropped sample: dist = %d, i = %d", dist, i);
				if (j->path && j->path->stats)
					stats_update(j->path->stats->delta, STATS_DROPPED, dist);
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

static struct plugin p = {
	.name		= "drop",
	.description	= "Drop messages with reordered sequence numbers",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 3,
		.history = 1,
		.cb	= hook_drop,
		.type	= HOOK_AUTO | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */