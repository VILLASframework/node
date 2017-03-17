/** Decimate hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"

static int hook_decimate(struct hook *h, int when, struct hook_info *j)
{
	struct {
		unsigned ratio;
		unsigned counter;
	} *private = hook_storage(h, when, sizeof(*private), NULL, NULL);

	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook: '%s'", plugin_name(h->_vt));
	
			private->ratio = strtol(h->parameter, NULL, 10);
			if (!private->ratio)
				error("Invalid parameter '%s' for hook 'decimate'", h->parameter);
		
			private->counter = 0;
			break;
		
		case HOOK_READ:
			assert(j->samples);
		
			int i, ok;
			for (i = 0, ok = 0; i < j->count; i++) {
				if (private->counter++ % private->ratio == 0) {
					struct sample *tmp;
					
					tmp = j->samples[ok];
					j->samples[ok++] = j->samples[i];
					j->samples[i] = tmp;
				}
			}

			return ok;
	}

	return 0;
}

static struct plugin p = {
	.name		= "decimate",
	.description	= "Downsamping by integer factor",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_decimate,
		.when	= HOOK_STORAGE | HOOK_DESTROY | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */