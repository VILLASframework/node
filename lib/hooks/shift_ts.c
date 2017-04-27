/** Shift timestamps of samples.
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

#include "hook.h"
#include "plugin.h"
#include "timing.h"

struct shift_ts {
	struct timespec offset;
	enum {
		SHIFT_ORIGIN,
		SHIFT_RECEIVED,
		SHIFT_SENT,
	} mode;
};

static int shift_ts_init(struct hook *h)
{
	struct shift_ts *p = h->_vd;
	
	p->mode = SHIFT_ORIGIN; /* Default mode */
	
	return 0;
}

static int shift_ts_parse(struct hook *h, config_setting_t *cfg)
{
	struct shift_ts *p = h->_vd;

	const char *mode;

	if (config_setting_lookup_string(cfg, "mode", &mode)) {
		if      (!strcmp(mode, "origin"))
			p->mode = SHIFT_ORIGIN;
		else if (!strcmp(mode, "received"))
			p->mode = SHIFT_RECEIVED;
		else if (!strcmp(mode, "sent"))
			p->mode = SHIFT_SENT;
		else
			cerror(cfg, "Invalid mode parameter '%s' for hook '%s'", mode, plugin_name(h->_vt));
	}
	
	double offset;
	if (!config_setting_lookup_float(cfg, "offset", &offset))
		cerror(cfg, "Missing setting 'offset' for hook '%s'", plugin_name(h->_vt));
			
	p->offset = time_from_double(offset);
	
	return 0;
}

static int shift_ts_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct shift_ts *p = h->_vd;
	
	for (int i = 0; i < *cnt; i++) {
		struct sample *s = smps[i];
		struct timespec *ts;

		switch (p->mode) {
			case SHIFT_ORIGIN: ts = &s->ts.origin; break;
			case SHIFT_RECEIVED: ts = &s->ts.received; break;
			case SHIFT_SENT: ts = &s->ts.sent; break;
			default: return -1;
		}
		
		*ts = time_add(ts, &p->offset); break;
	}

	return 0;
}

static struct plugin p = {
	.name		= "shift_ts",
	.description	= "Shift timestamps of samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.init	= shift_ts_init,
		.parse	= shift_ts_parse,
		.read	= shift_ts_read,
		.size	= sizeof(struct shift_ts)
	}
};

REGISTER_PLUGIN(&p)

/** @} */