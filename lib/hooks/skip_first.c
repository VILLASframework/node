/** Skip first hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

static int skip_first_parse(struct hook *h, config_setting_t *cfg)
{
	struct skip_first *p = h->_vd;

	double seconds;

	if (config_setting_lookup_float(cfg, "seconds", &seconds)) {
		p->seconds.wait = time_from_double(seconds);
		p->mode = HOOK_SKIP_MODE_SECONDS;
	}
	else if (config_setting_lookup_int(cfg, "samples", &p->samples.wait)) {
		p->mode = HOOK_SKIP_MODE_SAMPLES;
	}
	else
		cerror(cfg, "Missing setting 'seconds' or 'samples' for hook '%s'", plugin_name(h->_vt));
	
	return 0;
}

static int skip_first_restart(struct hook *h)
{
	struct skip_first *p = h->_vd;

	p->state = HOOK_SKIP_FIRST_STATE_STARTED;
	
	return 0;
}

static int skip_first_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct skip_first *p = h->_vd;

	/* Remember sequence no or timestamp of first sample. */
	if (p->state == HOOK_SKIP_FIRST_STATE_STARTED) {
		switch (p->mode) {
			case HOOK_SKIP_MODE_SAMPLES:
				p->samples.until = smps[0]->sequence + p->samples.wait;
				break;
			
			case HOOK_SKIP_MODE_SECONDS:
				p->seconds.until = time_add(&smps[0]->ts.origin, &p->seconds.wait);
				break;
		}
		
		p->state = HOOK_SKIP_FIRST_STATE_SKIPPING;
	}

	int i, ok;
	for (i = 0, ok = 0; i < *cnt; i++) {
		bool skip;
		switch (p->mode) {
			case HOOK_SKIP_MODE_SAMPLES:
				skip = p->samples.until > smps[i]->sequence;
				break;
			
			case HOOK_SKIP_MODE_SECONDS:
				skip = time_delta(&p->seconds.until, &smps[i]->ts.origin) < 0;
				break;
			default:
				skip = false;
				break;	
		}
		
		if (!skip) {
			struct sample *tmp;

			tmp = smps[i];
			smps[i] = smps[ok];
			smps[ok++] = tmp;
		}

		/* To discard the first X samples in 'smps[]' we must
		 * shift them to the end of the 'smps[]' array.
		 * In case the hook returns a number 'ok' which is smaller than 'cnt',
		 * only the first 'ok' samples in 'smps[]' are accepted and further processed.
		 */
	}
	
	*cnt = ok;

	return 0;
}

static struct plugin p = {
	.name		= "skip_first",
	.description	= "Skip the first samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.parse	= skip_first_parse,
		.start	= skip_first_restart,
		.restart = skip_first_restart,
		.read	= skip_first_read,
		.size	= sizeof(struct skip_first)
	}
};

REGISTER_PLUGIN(&p)
	
/** @} */