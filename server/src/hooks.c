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

#include "timing.h"
#include "config.h"
#include "msg.h"
#include "hooks.h"
#include "path.h"
#include "utils.h"

/* Some hooks can be configured by constants in te file "config.h" */

/** This is a static list of available hooks. */
struct list hooks;

REGISTER_HOOK("print", 99, hook_print, HOOK_MSG)
int hook_print(struct path *p)
{
	struct msg *m = p->current;
	struct timespec ts = MSG_TS(m);

	fprintf(stdout, "%.3e+", time_delta(&ts, &p->ts_recv)); /* Print delay */
	msg_fprint(stdout, m);

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

REGISTER_HOOK("fir", 99, hook_fir, HOOK_POST)
int hook_fir(struct path *p)
{
	/** Simple FIR-LP: F_s = 1kHz, F_pass = 100 Hz, F_block = 300
	 * Tip: Use MATLAB's filter design tool and export coefficients
	 *      with the integrated C-Header export */
	static const double coeffs[] = {
		-0.003658148158728, -0.008882653268281, 0.008001024183003,
		0.08090485991761,    0.2035239551043,   0.3040703593515,
		0.3040703593515,     0.2035239551043,   0.08090485991761,
		0.008001024183003,  -0.008882653268281,-0.003658148158728 };

	/* Accumulator */
	double sum = 0;

	/** Trim FIR length to length of history buffer */
	int len = MIN(ARRAY_LEN(coeffs), p->poolsize);

	for (int i = 0; i < len; i++) {
		struct msg *old = &p->pool[(p->poolsize+p->received-i) % p->poolsize];

		sum += coeffs[i] * old->data[HOOK_FIR_INDEX].f;
	}

	p->current->data[HOOK_FIR_INDEX].f = sum;

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
		char buf[33];
		path_print(p, buf, sizeof(buf));
		warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
			buf, p->previous->sequence, p->current->sequence);

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
