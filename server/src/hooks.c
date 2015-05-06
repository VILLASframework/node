/** Hook funktions
 *
 * Every path can register a hook function which is called for every received
 * message. This can be used to debug the data flow, get statistics
 * or alter the message.
 *
 * This file includes some examples.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "msg.h"
#include "hooks.h"
#include "path.h"
#include "utils.h"

/** @todo Make const */
static struct hook_id hook_list[] = {
	{ hook_print, "print" },
	{ hook_decimate, "decimate" },
	{ hook_tofixed, "tofixed" },
	{ hook_ts, "ts" },
	{ hook_fir, "fir" },
	{ hook_dft, "dft"},
	{ hook_multiplex, "multiplex" },
	{ hook_demultiplex, "demultiplex" },
};

hook_cb_t hook_lookup(const char *name)
{
	for (int i=0; i<ARRAY_LEN(hook_list); i++) {
		if (!strcmp(name, hook_list[i].name))
			return hook_list[i].cb;
	}

	return NULL; /* No matching hook was found */
}
 
int hook_print(struct msg *m, struct path *p)
{
	/* Print every message once to stdout */
	msg_fprint(stdout, m);

	return 0;
}

int hook_tofixed(struct msg *m, struct path *p)
{
	for (int i=0; i<m->length; i++) {
		m->data[i].i = m->data[i].f * 1e3; 
	}
	
	return 0;
}

int hook_ts(struct msg *m, struct path *p)
{
	struct timespec *ts = (struct timespec *) &m->data[HOOK_TS_INDEX];
	
	clock_gettime(CLOCK_REALTIME, ts);
	
	return 0;
}

int hook_fir(struct msg *m, struct path *p)
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
	int len = MIN(ARRAY_LEN(coeffs), POOL_SIZE);

	for (int i=0; i<len; i++) {
		struct msg *old = &p->history[(POOL_SIZE+p->received-i) % POOL_SIZE];
		
		sum += coeffs[i] * old->data[HOOK_FIR_INDEX].f;
	}

	m->data[HOOK_FIR_INDEX].f = sum;

	return 0;
}

int hook_decimate(struct msg *m, struct path *p)
{
	/* Drop every HOOK_DECIMATE_RATIO'th message */
	return (m->sequence % HOOK_DECIMATE_RATIO == 0) ? -1 : 0;
}

int hook_multiplex(struct msg *m, struct path *p)
{
	/* Every HOOK_MULTIPLEX_RATIO'th message contains the collection of the previous ones */
	if (p->received % HOOK_MULTIPLEX_RATIO == 0) {
		struct msg *c = p->current; /* Current message */

		for (int i=1; i<HOOK_MULTIPLEX_RATIO && c->length<MSG_VALUES; i++) {
			struct msg *m = p->history[p->received-i];
			
			/* Trim amount of values to actual size of msg buffer */
			int len = MIN(m->length, MSG_VALUES - c->length);
			
			memcpy(c->data + c->length, m->data, len * sizeof(m->data[0]));
			c->length += len; 
		}
		
		return 0;
	}
	else
		return -1; /* Message will be dropped */
}

int hook_demultiplex(struct msg *m, struct path *p)
{
	
}

int hook_dft(struct msg *m, struct path *p)
{
	/** @todo Implement */
}