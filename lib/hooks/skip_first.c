/** Skip first hook.
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

static int hook_skip_first(struct hook *h, int when, struct hook_info *j)
{
	struct {
		struct timespec skip;	/**< Time to wait until first message is not skipped */
		struct timespec until;	/**< Absolute point in time from where we accept samples. */
		enum {
			HOOK_SKIP_FIRST_STATE_STARTED,	/**< Path just started. First sample not received yet. */
			HOOK_SKIP_FIRST_STATE_SKIPPING,	/**< First sample received. Skipping samples now. */
			HOOK_SKIP_FIRST_STATE_NORMAL,	/**< All samples skipped. Normal operation. */
		} state;
	} *private = hook_storage(h, when, sizeof(*private), NULL, NULL);

	char *endptr;
	double wait;

	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook: '%s'", plugin_name(h->_vt));

			wait = strtof(h->parameter, &endptr);
			if (h->parameter == endptr)
				error("Invalid parameter '%s' for hook 'skip_first'", h->parameter);
	
			private->skip = time_from_double(wait);
			break;
		
		case HOOK_PATH_START:
		case HOOK_PATH_RESTART:
			private->state = HOOK_SKIP_FIRST_STATE_STARTED;
			break;
	
		case HOOK_READ:
			assert(j->samples);
			
			if (private->state == HOOK_SKIP_FIRST_STATE_STARTED) {
				private->until = time_add(&j->samples[0]->ts.received, &private->skip);
				private->state = HOOK_SKIP_FIRST_STATE_SKIPPING;
			}
			

			int i, ok;
			for (i = 0, ok = 0; i < j->count; i++) {
				if (time_delta(&private->until, &j->samples[i]->ts.received) > 0) {
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

			return ok;
	}

	return 0;
}

static struct plugin p = {
	.name		= "skip_first",
	.description	= "Skip the first samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_skip_first,
		.when	= HOOK_STORAGE |  HOOK_PARSE | HOOK_READ | HOOK_PATH
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */