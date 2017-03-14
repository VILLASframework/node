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

static struct plugin p = {
	.name		= "fix_ts",
	.description	= "Update timestamps of sample if not set",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 0,
		.cb	= hook_fix_ts,
		.type	= HOOK_AUTO | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */