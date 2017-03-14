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

static int hook_shift(struct hook *h, int when, struct hook_info *k)
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

	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook: '%s'", plugin_name(h));

			char *endptr, *off;
			
			char *cpy = strdup(h->parameter);
			char *tok1 = strtok(cpy, ",");
			char *tok2 = strtok(NULL, ",");

			if (tok2) {
				off = tok2;

				if      (!strcmp(tok1, "origin"))
					private->mode = SHIFT_TS_ORIGIN;
				else if (!strcmp(tok1, "received"))
					private->mode = SHIFT_TS_RECEIVED;
				else if (!strcmp(tok1, "sent"))
					private->mode = SHIFT_TS_SENT;
				else if (!strcmp(tok1, "sequence"))
					private->mode = SHIFT_SEQUENCE;
				else
					error("Invalid mode parameter for hook '%s'", plugin_name(h));
			}
			else {
				off = tok1;

				private->mode = SHIFT_TS_ORIGIN; /* Default mode */
			}
			
			switch (private->mode) {
				case SHIFT_TS_ORIGIN:
				case SHIFT_TS_RECEIVED:
				case SHIFT_TS_SENT:
					private->offset.ts = time_from_double(strtod(off, &endptr));
					break;

				case SHIFT_SEQUENCE:
					private->offset.seq = strtoul(off, &endptr, 10);
					break;
			}
			
			if (endptr == off)
				error("Invalid offset parameter for hook '%s'", plugin_name(h));
			
			free(cpy);

			break;
		
		case HOOK_READ:
			for (int i = 0; i < k->cnt; i++) {
				struct sample *s = k->smps[i];

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

			return k->cnt;
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
		.type	= HOOK_STORAGE | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */