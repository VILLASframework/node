/** Hook funktions
 *
 * Every path can register hook functions which are called at specific events.
 * A list of supported events is described by enum hook_flags.
 * Please note that there are several hook callbacks which are hard coded into path_create().
 *
 * This file includes some examples.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>
#include <math.h>

#include "timing.h"
#include "config.h"
#include "msg.h"
#include "hooks.h"
#include "path.h"
#include "utils.h"

/* Some hooks can be configured by constants in te file "config.h" */

struct list hooks;

REGISTER_HOOK("deduplicate", 99, hook_deduplicate, HOOK_DEDUP_TYPE)
int hook_deduplicate(struct path *p)
{
	int ret = 0;
#if HOOK_DEDUP_TYPE == HOOK_ASYNC
	/** Thread local storage (TLS) is used to maintain a copy of the last run of the hook */
	static __thread struct msg previous = MSG_INIT(0);
	struct msg *prev = &previous;
#else
	struct msg *prev = p->previous;
#endif
	struct msg *cur = p->current;
	
	for (int i = 0; i < MIN(cur->length, prev->length); i++) {
		if (fabs(cur->data[i].f - prev->data[i].f) > HOOK_DEDUP_TRESH)
			goto out;
	}
	
	ret = -1; /* no appreciable change in values, we will drop the packet */

out:
#if HOOK_DEDUP_TYPE == HOOK_ASYNC
	memcpy(prev, cur, sizeof(struct msg)); /* save current message for next run */
#endif
	return ret;
}

REGISTER_HOOK("print", 99, hook_print, HOOK_MSG)
int hook_print(struct path *p)
{
	struct msg *m = p->current;
	struct timespec ts = MSG_TS(m);

	msg_fprint(stdout, m, MSG_PRINT_ALL, time_delta(&ts, &p->ts_recv));

	return 0;
}

REGISTER_HOOK("tofixed", 99, hook_tofixed, HOOK_MSG)
int hook_tofixed(struct path *p)
{
	struct msg *m = p->current;

	for (int i = 0; i < m->length; i++)
		m->data[i].i = m->data[i].f * 1e3;

	return 0;
}

REGISTER_HOOK("ts", 99,	hook_ts, HOOK_MSG)
int hook_ts(struct path *p)
{
	struct msg *m = p->current;

	m->ts.sec = p->ts_recv.tv_sec;
	m->ts.nsec = p->ts_recv.tv_nsec;

	return 0;
}

REGISTER_HOOK("fir", 99, hook_fir, HOOK_MSG)
int hook_fir(struct path *p)
{
	/** Coefficients for simple FIR-LowPass:
	 *   F_s = 1kHz, F_pass = 100 Hz, F_block = 300
	 *
	 * Tip: Use MATLAB's filter design tool and export coefficients
	 *      with the integrated C-Header export
	 */
	static const double coeffs[] = HOOK_FIR_COEFFS;
	
	/** Per path thread local storage for unfiltered sample values.
	 * The message ringbuffer (p->pool & p->current) will contain filtered data!
	 */
	static __thread double *past = NULL;
	
	/** @todo Avoid dynamic allocation at runtime */
	if (!past)
		alloc(p->poolsize * sizeof(double));
	
	
	/* Current value of interest */
	float *cur = &p->current->data[HOOK_FIR_INDEX].f;
	
	/* Save last sample, unfiltered */
	past[p->received % p->poolsize] = *cur;

	/* Reset accumulator */
	*cur = 0;
	
	/* FIR loop */
	for (int i = 0; i < MIN(ARRAY_LEN(coeffs), p->poolsize); i++)
		*cur += coeffs[i] * past[p->received+p->poolsize-i];

	return 0;
}

REGISTER_HOOK("decimate", 99, hook_decimate, HOOK_POST)
int hook_decimate(struct path *p)
{
	/* Only sent every HOOK_DECIMATE_RATIO'th message */
	return p->received % HOOK_DECIMATE_RATIO;
}

REGISTER_HOOK("dft", 99, hook_dft, HOOK_POST)
int hook_dft(struct path *p)
{
	return 0; /** @todo Implement */
}

/** System hooks */

int hook_restart(struct path *p)
{
	if (p->current->sequence  == 0 &&
	    p->previous->sequence <= UINT32_MAX - 32) {
		char *buf = path_print(p);
		warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
			buf, p->previous->sequence, p->current->sequence);
		free(buf);

		path_reset(p);
	}

	return 0;
}

int hook_verify(struct path *p)
{
	int reason = msg_verify(p->current);
	if (reason) {
		p->invalid++;
		warn("Received invalid message (reason=%d)", reason);

		return -1;
	}

	return 0;
}

int hook_drop(struct path *p)
{
	int dist = p->current->sequence - (int32_t) p->previous->sequence;
	if (dist <= 0 && p->received > 1) {
		p->dropped++;
		return -1;
	}
	else
		return 0;
}
