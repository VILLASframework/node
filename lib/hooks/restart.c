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

static int restart_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	assert(h->path);

	for (int i = 0; i < *cnt; i++) {
		h->last = smps[i];

		if (h->prev) {
			if (h->last->sequence  == 0 &&
			    h->prev->sequence <= UINT32_MAX - 32) {
				warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
					path_name(h->path), h->prev->sequence, h->last->sequence);

				/* Run restart hooks */
				for (size_t i = 0; i < list_length(&h->path->hooks); i++) {
					struct hook *k = list_at(&h->path->hooks, i);
					
					hook_restart(k);
				}
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
		.builtin = true,
		.read	= restart_read
	}
};

REGISTER_PLUGIN(&p)

/** @} */