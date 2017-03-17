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

static int hook_shift(struct hook *h, int when, struct hook_info *j)
{
	struct {
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
	} *private = hook_storage(h, when, sizeof(*private), NULL, NULL);
	
	const char *mode;

	switch (when) {
		case HOOK_INIT:
			private->mode = SHIFT_TS_ORIGIN; /* Default mode */
			break;

		case HOOK_PARSE:
			if (!h->cfg)
				error("Missing configuration for hook: '%s'", plugin_name(h->_vt));

			if (config_setting_lookup_string(h->cfg, "mode", &mode)) {
				if      (!strcmp(mode, "origin"))
					private->mode = SHIFT_TS_ORIGIN;
				else if (!strcmp(mode, "received"))
					private->mode = SHIFT_TS_RECEIVED;
				else if (!strcmp(mode, "sent"))
					private->mode = SHIFT_TS_SENT;
				else if (!strcmp(mode, "sequence"))
					private->mode = SHIFT_SEQUENCE;
				else
					error("Invalid mode parameter '%s' for hook '%s'", mode, plugin_name(h->_vt));
			}
			
			switch (private->mode) {
				case SHIFT_TS_ORIGIN:
				case SHIFT_TS_RECEIVED:
				case SHIFT_TS_SENT: {
					double offset;
					
					if (!config_setting_lookup_float(h->cfg, "offset", &offset))
						cerror(h->cfg, "Missing setting 'offset' for hook '%s'", plugin_name(h->_vt));
					
					private->offset.ts = time_from_double(offset);
					break;
				}

				case SHIFT_SEQUENCE: {
					int offset;
					
					if (!config_setting_lookup_int(h->cfg, "offset", &offset))
						cerror(h->cfg, "Missing setting 'offset' for hook '%s'", plugin_name(h->_vt));
					
					private->offset.seq = offset;
					break;
				}
			}
			
			break;
		
		case HOOK_READ:
			for (int i = 0; i < j->count; i++) {
				struct sample *s = j->samples[i];

				switch (private->mode) {
					case SHIFT_TS_ORIGIN:
						s->ts.origin = time_add(&s->ts.origin, &private->offset.ts); break;
					case SHIFT_TS_RECEIVED:
						s->ts.received = time_add(&s->ts.received, &private->offset.ts); break;
					case SHIFT_TS_SENT:
						s->ts.origin = time_add(&s->ts.sent, &private->offset.ts); break;
					case SHIFT_SEQUENCE:
						s->sequence += private->offset.seq; break;
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
		.cb	= hook_shift,
		.when	= HOOK_STORAGE | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */