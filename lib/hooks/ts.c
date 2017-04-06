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

static int ts_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	for (int i = 0; i < *cnt; i++)
		smps[i]->ts.origin = smps[i]->ts.received;

	return 0;
}

static struct plugin p = {
	.name		= "ts",
	.description	= "Overwrite origin timestamp of samples with receive timestamp",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.read	= ts_read,
		.size	= 0
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */