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

int fix_ts_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct timespec now;

	now = time_now();

	for (int i = 0; i < *cnt; i++) {
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

	return 0;
}

static struct plugin p = {
	.name		= "fix_ts",
	.description	= "Update timestamps of sample if not set",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 0,
		.builtin = true,
		.read	= fix_ts_read,
	}
};

REGISTER_PLUGIN(&p)

/** @} */