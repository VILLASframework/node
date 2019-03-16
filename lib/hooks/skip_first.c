/** Skip first hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/timing.h>
#include <villas/sample.h>

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

static int skip_first_parse(struct hook *h, json_t *cfg)
{
	struct skip_first *p = (struct skip_first *) h->_vd;

	double seconds;

	int ret;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: F }", "seconds", &seconds);
	if (!ret) {
		p->seconds.wait = time_from_double(seconds);
		p->mode = HOOK_SKIP_MODE_SECONDS;

		return 0;
	}

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i }", "samples", &p->samples.wait);
	if (!ret) {
		p->mode = HOOK_SKIP_MODE_SAMPLES;

		return 0;
	}

	jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(h->_vt));

	return -1;
}

static int skip_first_restart(struct hook *h)
{
	struct skip_first *p = (struct skip_first *) h->_vd;

	p->state = HOOK_SKIP_FIRST_STATE_STARTED;

	return 0;
}

static int skip_first_process(struct hook *h, struct sample *smp)
{
	struct skip_first *p = (struct skip_first *) h->_vd;

	/* Remember sequence no or timestamp of first sample. */
	if (p->state == HOOK_SKIP_FIRST_STATE_STARTED) {
		switch (p->mode) {
			case HOOK_SKIP_MODE_SAMPLES:
				p->samples.until = smp->sequence + p->samples.wait;
				break;

			case HOOK_SKIP_MODE_SECONDS:
				p->seconds.until = time_add(&smp->ts.origin, &p->seconds.wait);
				break;
		}

		p->state = HOOK_SKIP_FIRST_STATE_SKIPPING;
	}

	switch (p->mode) {
		case HOOK_SKIP_MODE_SAMPLES:
			if (p->samples.until > smp->sequence)
				return HOOK_SKIP_SAMPLE;
			break;

		case HOOK_SKIP_MODE_SECONDS:
			if (time_delta(&p->seconds.until, &smp->ts.origin) < 0)
				return HOOK_SKIP_SAMPLE;
			break;

		default:
			break;
	}

	return HOOK_OK;
}

static struct plugin p = {
	.name		= "skip_first",
	.description	= "Skip the first samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ | HOOK_NODE_WRITE | HOOK_PATH,
		.priority	= 99,
		.parse		= skip_first_parse,
		.start		= skip_first_restart,
		.restart	= skip_first_restart,
		.process	= skip_first_process,
		.size		= sizeof(struct skip_first)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
