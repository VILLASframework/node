/** Timestamp hook.
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

static int hook_ts(struct hook *h, int when, struct hook_info *j)
{
	assert(j->samples);

	for (int i = 0; i < j->count; i++)
		j->samples[i]->ts.origin = j->samples[i]->ts.received;

	return j->count;
}

static struct plugin p = {
	.name		= "ts",
	.description	= "Update timestamp of message with current time",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_ts,
		.when	= HOOK_READ
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */