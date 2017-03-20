/** Time shift hook.
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

struct shift {
	union {
		struct timespec ts;	/**< For SHIFT_TS_* modes. */
		int seq;		/**< For SHIFT_SEQUENCE mode. */
	} offset;
	enum {
		SHIFT_TS_ORIGIN,
		SHIFT_TS_RECEIVED,
		SHIFT_TS_SENT,
		SHIFT_SEQUENCE
	} mode;
};

static int hook_shift(struct hook *h, int when, struct hook_info *j)
{
	struct shift *p = (struct shift *) h->_vd;
	
	const char *mode;

	switch (when) {
		case HOOK_INIT:
			p->mode = SHIFT_TS_ORIGIN; /* Default mode */
			break;

		case HOOK_PARSE:
			if (!h->cfg)
				error("Missing configuration for hook: '%s'", plugin_name(h->_vt));

			if (config_setting_lookup_string(h->cfg, "mode", &mode)) {
				if      (!strcmp(mode, "origin"))
					p->mode = SHIFT_TS_ORIGIN;
				else if (!strcmp(mode, "received"))
					p->mode = SHIFT_TS_RECEIVED;
				else if (!strcmp(mode, "sent"))
					p->mode = SHIFT_TS_SENT;
				else if (!strcmp(mode, "sequence"))
					p->mode = SHIFT_SEQUENCE;
				else
					error("Invalid mode parameter '%s' for hook '%s'", mode, plugin_name(h->_vt));
			}
			
			switch (p->mode) {
				case SHIFT_TS_ORIGIN:
				case SHIFT_TS_RECEIVED:
				case SHIFT_TS_SENT: {
					double offset;
					
					if (!config_setting_lookup_float(h->cfg, "offset", &offset))
						cerror(h->cfg, "Missing setting 'offset' for hook '%s'", plugin_name(h->_vt));
					
					p->offset.ts = time_from_double(offset);
					break;
				}

				case SHIFT_SEQUENCE: {
					int offset;
					
					if (!config_setting_lookup_int(h->cfg, "offset", &offset))
						cerror(h->cfg, "Missing setting 'offset' for hook '%s'", plugin_name(h->_vt));
					
					p->offset.seq = offset;
					break;
				}
			}
			
			break;
		
		case HOOK_READ:
			for (int i = 0; i < j->count; i++) {
				struct sample *s = j->samples[i];

				switch (p->mode) {
					case SHIFT_TS_ORIGIN:
						s->ts.origin = time_add(&s->ts.origin, &p->offset.ts); break;
					case SHIFT_TS_RECEIVED:
						s->ts.received = time_add(&s->ts.received, &p->offset.ts); break;
					case SHIFT_TS_SENT:
						s->ts.origin = time_add(&s->ts.sent, &p->offset.ts); break;
					case SHIFT_SEQUENCE:
						s->sequence += p->offset.seq; break;
				}
			}

			return j->count;
	}

	return 0;
}

static struct plugin p = {
	.name		= "shift",
	.description	= "Shift the origin timestamp or sequence number of samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.size	= sizeof(struct shift),
		.cb	= hook_shift,
		.when	= HOOK_STORAGE | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */