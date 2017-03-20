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

struct decimate {
	int ratio;
	unsigned counter;
};

static int hook_decimate(struct hook *h, int when, struct hook_info *j)
{
	struct decimate *p = (struct decimate *) h->_vd;

	switch (when) {
		case HOOK_INIT:
			p->counter = 0;
			break;
		
		case HOOK_PARSE:
			if (!h->cfg)
				error("Missing configuration for hook: '%s'", plugin_name(h->_vt));
		
			if (!config_setting_lookup_int(h->cfg, "ratio", &p->ratio))
				cerror(h->cfg, "Missing setting 'ratio' for hook '%s'", plugin_name(h->_vt));
	
			break;
		
		case HOOK_READ:
			assert(j->samples);
		
			int i, ok;
			for (i = 0, ok = 0; i < j->count; i++) {
				if (p->counter++ % p->ratio == 0) {
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
		.size	= sizeof(struct decimate),
		.cb	= hook_decimate,
		.when	= HOOK_STORAGE | HOOK_PARSE | HOOK_DESTROY | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */