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
	assert(j->smps);

	for (int i = 0; i < j->cnt; i++)
		j->smps[i]->ts.origin = j->smps[i]->ts.received;

	return j->cnt;
}

static struct plugin p = {
	.name		= "ts",
	.description	= "Update timestamp of message with current time",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.history = 0,
		.cb	= hook_ts,
		.type	= HOOK_READ
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */