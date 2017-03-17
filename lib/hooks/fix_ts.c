/** Fix timestamp hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"
#include "timing.h"

int hook_fix_ts(struct hook *h, int when, struct hook_info *j)
{
	struct timespec now = time_now();

	assert(j->samples);

	for (int i = 0; i < j->count; i++) {
		/* Check for missing receive timestamp
		 * Usually node_type::read() should update the receive timestamp.
		 * An example would be to use hardware timestamp capabilities of
		 * modern NICs.
		 */
		if ((j->samples[i]->ts.received.tv_sec ==  0 && j->samples[i]->ts.received.tv_nsec ==  0) ||
		    (j->samples[i]->ts.received.tv_sec == -1 && j->samples[i]->ts.received.tv_nsec == -1))
			j->samples[i]->ts.received = now;

		/* Check for missing origin timestamp */
		if ((j->samples[i]->ts.origin.tv_sec ==  0 && j->samples[i]->ts.origin.tv_nsec ==  0) ||
		    (j->samples[i]->ts.origin.tv_sec == -1 && j->samples[i]->ts.origin.tv_nsec == -1))
			j->samples[i]->ts.origin = now;
	}

	return j->count;
}

static struct plugin p = {
	.name		= "fix_ts",
	.description	= "Update timestamps of sample if not set",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 0,
		.cb	= hook_fix_ts,
		.when	= HOOK_AUTO | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */