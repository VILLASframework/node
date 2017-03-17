/** Print hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"
#include "sample.h"

static int hook_print(struct hook *h, int when, struct hook_info *j)
{
	assert(j->samples);
	
	for (int i = 0; i < j->count; i++)
		sample_fprint(stdout, j->samples[i], SAMPLE_ALL);

	return j->count;
}

static struct plugin p = {
	.name		= "print",
	.description	= "Print the message to stdout",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_print,
		.when	= HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */