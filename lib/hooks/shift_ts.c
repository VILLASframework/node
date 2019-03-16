/** Shift timestamps of samples.
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

#include <string.h>

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/timing.h>
#include <villas/sample.h>

struct shift_ts {
	struct timespec offset;
	enum {
		SHIFT_ORIGIN,
		SHIFT_RECEIVED
	} mode;
};

static int shift_ts_init(struct hook *h)
{
	struct shift_ts *p = (struct shift_ts *) h->_vd;

	p->mode = SHIFT_ORIGIN; /* Default mode */

	return 0;
}

static int shift_ts_parse(struct hook *h, json_t *cfg)
{
	struct shift_ts *p = (struct shift_ts *) h->_vd;
	double offset;
	const char *mode = NULL;
	int ret;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: F }",
		"mode", &mode,
		"offset", &offset
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(h->_vt));

	if (mode) {
		if      (!strcmp(mode, "origin"))
			p->mode = SHIFT_ORIGIN;
		else if (!strcmp(mode, "received"))
			p->mode = SHIFT_RECEIVED;
		else
			jerror(&err, "Invalid mode parameter '%s' for hook '%s'", mode, hook_type_name(h->_vt));
	}

	p->offset = time_from_double(offset);

	return 0;
}

static int shift_ts_process(struct hook *h, struct sample *smp)
{
	struct shift_ts *p = (struct shift_ts *) h->_vd;

	struct timespec *ts;

	switch (p->mode) {
		case SHIFT_ORIGIN:
			ts = &smp->ts.origin;
			break;

		case SHIFT_RECEIVED:
			ts = &smp->ts.received;
			break;

		default:
			return HOOK_ERROR;
	}

	*ts = time_add(ts, &p->offset);;

	return HOOK_OK;
}

static struct plugin p = {
	.name		= "shift_ts",
	.description	= "Shift timestamps of samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ | HOOK_PATH,
		.priority 	= 99,
		.init		= shift_ts_init,
		.parse		= shift_ts_parse,
		.process	= shift_ts_process,
		.size		= sizeof(struct shift_ts)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
