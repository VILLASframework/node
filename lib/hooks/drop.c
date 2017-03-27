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

static int drop_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	int i, ok, dist;
	
	for (i = 0, ok = 0; i < *cnt; i++) {
		h->last = smps[i];
		
		if (h->prev) {
			dist = h->last->sequence - (int32_t) h->prev->sequence;
			if (dist <= 0) {
				warn("Dropped sample: dist = %d, i = %d", dist, i);
				if (h->path && h->path->stats)
					stats_update(h->path->stats->delta, STATS_REORDERED, dist);
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

	*cnt = ok;
		
	return 0;
}

static struct plugin p = {
	.name		= "drop",
	.description	= "Drop messages with reordered sequence numbers",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 3,
		.builtin = true,
		.read	= drop_read
	}
};

REGISTER_PLUGIN(&p)

/** @} */