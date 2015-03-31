/** Hook funktions
 *
 * Every path can register a hook function which is called for every received
 * message. This can be used to debug the data flow, get statistics
 * or alter the message.
 *
 * This file includes some examples.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
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
	{ NULL }
};

hook_cb_t hook_lookup(const char *name)
{
	for (struct hook_id *hid = hook_list; hid->cb; hid++) {
		if (!strcmp(name, hid->name)) {
			return hid->cb;
		}
	}

	return NULL; /* No matching hook was found */
}
 
int hook_print(struct msg *m, struct path *p)
{
	/* Print every message once to stdout */
	msg_fprint(stdout, m);

	return 0;
}

int hook_decimate(struct msg *m, struct path *p)
{
	/* Drop every HOOK_DECIMATE_RATIO'th message */
	return (m->sequence % HOOK_DECIMATE_RATIO == 0) ? -1 : 0;
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

/** Simple FIR-LP: F_s = 1kHz, F_pass = 100 Hz, F_block = 300
 * Tip: Use MATLAB's filter design tool and export coefficients
 *      with the integrated C-Header export */ 
static const double hook_fir_coeffs[] = { -0.003658148158728, -0.008882653268281, 0.008001024183003,
					   0.08090485991761,    0.2035239551043,   0.3040703593515,
					   0.3040703593515,     0.2035239551043,   0.08090485991761,
					   0.008001024183003,  -0.008882653268281,-0.003658148158728 };

/** @todo: test */
int hook_fir(struct msg *m, struct path *p)
{
	static pthread_key_t pkey;
	float *history = pthread_getspecific(pkey);
	
	/** Length of impulse response */
	int len = ARRAY_LEN(hook_fir_coeffs);
	/** Current index in circular history buffer */
	int cur = m->sequence % len;
	/* Accumulator */
	double sum = 0;
		
	/* Create thread local storage for circular history buffer */
	if (!history) {
		history = alloc(len * sizeof(float));
		pthread_key_create(&pkey, free);
		pthread_setspecific(pkey, history);
	}
	
	/* Update circular buffer */
	history[cur] = m->data[HOOK_FIR_INDEX].f;
	
	for (int i=0; i<len; i++)
		sum += hook_fir_coeffs[(cur+len-i)%len] * history[(cur+i)%len];

	m->data[HOOK_FIR_INDEX].f = sum;

	return 0;
}
