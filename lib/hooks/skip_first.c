/** Skip first hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include <libconfig.h>

#include "hook.h"
#include "plugin.h"
#include "timing.h"

struct skip_first {
	enum {
		HOOK_SKIP_FIRST_STATE_STARTED,	/**< Path just started. First sample not received yet. */
		HOOK_SKIP_FIRST_STATE_SKIPPING,	/**< First sample received. Skipping samples now. */
		HOOK_SKIP_FIRST_STATE_NORMAL,	/**< All samples skipped. Normal operation. */
	} state;
	enum {
		HOOK_SKIP_MODE_SECONDS,
		HOOK_SKIP_MODE_SAMPLES
	} mode;
	union {
		struct {
			struct timespec until;
			struct timespec wait;	/**< Absolute point in time from where we accept samples. */
		} seconds;
		struct {
			int until;
			int wait;
		} samples;
	};
};

static int hook_skip_first(struct hook *h, int when, struct hook_info *j)
{
	struct skip_first *p = (struct skip_first *) h->_vd;

	switch (when) {
		case HOOK_PARSE: {
			double seconds;
			
			if (!h->cfg)
				error("Missing configuration for hook: '%s'", plugin_name(h->_vt));
			
			if (config_setting_lookup_float(h->cfg, "seconds", &seconds)) {
				p->seconds.wait = time_from_double(seconds);
				p->mode = HOOK_SKIP_MODE_SECONDS;
			}
			else if (config_setting_lookup_int(h->cfg, "samples", &p->samples.wait)) {
				p->mode = HOOK_SKIP_MODE_SAMPLES;
			}
			else
				cerror(h->cfg, "Missing setting 'seconds' or 'samples' for hook '%s'", plugin_name(h->_vt));

			break;
		}
		
		case HOOK_PATH_START:
		case HOOK_PATH_RESTART:
			p->state = HOOK_SKIP_FIRST_STATE_STARTED;
			break;
	
		case HOOK_READ:
			assert(j->samples);
			
			if (p->state == HOOK_SKIP_FIRST_STATE_STARTED) {
				switch (p->mode) {
					case HOOK_SKIP_MODE_SAMPLES:
						p->samples.until = j->samples[0]->sequence + p->samples.wait;
						break;
					
					case HOOK_SKIP_MODE_SECONDS:
						p->seconds.until = time_add(&j->samples[0]->ts.received, &p->seconds.wait);
						break;
				}
				
				p->state = HOOK_SKIP_FIRST_STATE_SKIPPING;
			}

			int i, ok;
			for (i = 0, ok = 0; i < j->count; i++) {
				bool skip;
				switch (p->mode) {
					case HOOK_SKIP_MODE_SAMPLES:
						skip = p->samples.until >= j->samples[i]->sequence;
						break;
					
					case HOOK_SKIP_MODE_SECONDS:
						skip = time_delta(&p->seconds.until, &j->samples[i]->ts.received) < 0;
						break;
					default:
						skip = false;
				}
				
				if (!skip) {
					struct sample *tmp;

					tmp = j->samples[i];
					j->samples[i] = j->samples[ok];
					j->samples[ok++] = tmp;
				}

				/* To discard the first X samples in 'smps[]' we must
				 * shift them to the end of the 'smps[]' array.
				 * In case the hook returns a number 'ok' which is smaller than 'cnt',
				 * only the first 'ok' samples in 'smps[]' are accepted and further processed.
				 */
			}
			
			j->count = ok;
	}

	return 0;
}

static struct plugin p = {
	.name		= "skip_first",
	.description	= "Skip the first samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.size	= sizeof(struct skip_first),
		.cb	= hook_skip_first,
		.when	= HOOK_STORAGE |  HOOK_PARSE | HOOK_READ | HOOK_PATH
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */