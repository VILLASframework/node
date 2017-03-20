/** Path restart hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"
#include "path.h"

static int hook_restart(struct hook *h, int when, struct hook_info *j)
{
	assert(j->samples);
	assert(j->path);

	for (int i = 0; i < j->count; i++) {
		h->last = j->samples[i];
		
		if (h->prev) {
			if (h->last->sequence  == 0 &&
			    h->prev->sequence <= UINT32_MAX - 32) {
				warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
					path_name(j->path), h->prev->sequence, h->last->sequence);

				path_run_hooks(j->path, HOOK_PATH_RESTART, &j->samples[i], j->count - i);
			}
		}
		
		h->prev = h->last;
	}

	return 0;
}

static struct plugin p = {
	.name		= "restart",
	.description	= "Call restart hooks for current path",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 1,
		.cb	= hook_restart,
		.when	= HOOK_AUTO | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */