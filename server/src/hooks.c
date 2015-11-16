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

extern struct list nodes;

struct list hooks;

REGISTER_HOOK("print", 99, hook_print, HOOK_MSG)
int hook_print(struct path *p, struct hook *h, int when)
{
	struct msg *m = p->current;
	double offset = time_delta(&MSG_TS(m), &p->ts_recv);
	int flags = MSG_PRINT_ALL;

	/* We dont show the offset if its to large */
	if (offset > 1e9)
		flags &= ~MSG_PRINT_OFFSET;
	
	msg_fprint(stdout, m, flags, offset);

	return 0;
}

REGISTER_HOOK("ts", 99,	hook_ts, HOOK_MSG)
int hook_ts(struct path *p, struct hook *h, int when)
{
	struct msg *m = p->current;

	m->ts.sec = p->ts_recv.tv_sec;
	m->ts.nsec = p->ts_recv.tv_nsec;

	return 0;
}

REGISTER_HOOK("fix_ts", 0, hook_fix_ts, HOOK_INTERNAL | HOOK_MSG)
int hook_fix_ts(struct path *p, struct hook *h, int when)
{
	struct msg *m = p->current;

	if ((m->ts.sec ==  0 && m->ts.nsec ==  0) ||
	    (m->ts.sec == -1 && m->ts.nsec == -1))
		    hook_ts(p, h, when);

	return 0;
}

REGISTER_HOOK("skip_unchanged", 99, hook_skip_unchanged, HOOK_PRIVATE | HOOK_ASYNC)
int hook_skip_unchanged(struct path *p, struct hook *h, int when)
{	
	struct private {
		double threshold;
		struct msg previous;
	} *private = h->private;	
	
	switch (when) {
		case HOOK_INIT:
			private = h->private = alloc(sizeof(struct private));
		
			if (!h->parameter)
				error("Missing parameter for hook 'deduplication'");

			private->threshold = strtof(h->parameter, NULL);
			if (!private->threshold)
				error("Failed to parse parameter '%s' for hook 'deduplication'", h->parameter);
			break;
			
		case HOOK_DEINIT:
			free(private);
			break;
			
		case HOOK_ASYNC: {
			int ret = 0;

			struct msg *prev = &private->previous;
			struct msg *cur = p->current;
	
			for (int i = 0; i < MIN(cur->length, prev->length); i++) {
				if (fabs(cur->data[i].f - prev->data[i].f) > private->threshold)
					goto out;
			}
	
			ret = -1; /* no appreciable change in values, we will drop the packet */

		out:	memcpy(prev, cur, sizeof(struct msg)); /* save current message for next run */

			return ret;
		}
	}
	
	return 0;
}

REGISTER_HOOK("convert", 99, hook_convert, HOOK_PRIVATE | HOOK_MSG)
int hook_convert(struct path *p, struct hook *h, int when)
{
	struct private {
		enum { TO_FIXED, TO_FLOAT } mode;
	} *private = h->private;
	
	switch (when) {
		case HOOK_INIT:
			private = h->private = alloc(sizeof(struct private));
		
			if (!h->parameter)
				error("Missing parameter for hook 'deduplication'");
			
			if      (!strcmp(h->parameter, "fixed"))
				private->mode = TO_FIXED;
			else if (!strcmp(h->parameter, "float"))
				private->mode = TO_FLOAT;
			else
				error("Invalid parameter '%s' for hook 'convert'", h->parameter);
			break;
			
		case HOOK_DEINIT:
			free(private);
			break;
			
		case HOOK_MSG: {
			struct msg *m = p->current;

			for (int i = 0; i < m->length; i++) {
				switch (private->mode) {
					/** @todo allow precission to be configured via parameter */
					case TO_FIXED: m->data[i].i = m->data[i].f * 1e3; break;
					case TO_FLOAT: m->data[i].f = m->data[i].i; break;
				}
			}
			break;
		}
	}

	return 0;
}

REGISTER_HOOK("fir", 99, hook_fir, HOOK_PRIVATE | HOOK_MSG)
int hook_fir(struct path *p, struct hook *h, int when)
{
	/** @todo make this configurable via hook parameters */
	const static double coeffs[] = HOOK_FIR_COEFFS;

	struct private {
		double *coeffs;
		double *history;
		int index;
	} *private = h->private;
	
	switch (when) {
		case HOOK_INIT:
			if (!h->parameter)
				error("Missing parameter for hook 'fir'");

			private = h->private = alloc(sizeof(struct private));
		
			private->coeffs = memdup(coeffs, sizeof(coeffs));
			private->history = alloc(sizeof(coeffs));

			private->index = strtol(h->parameter, NULL, 10);
			if (!private->index)
				error("Invalid parameter '%s' for hook 'fir'", h->parameter);
			break;

		case HOOK_DEINIT:
			free(private->coeffs);
			free(private->history);
			free(private);
			break;

		case HOOK_MSG: {
			/* Current value of interest */
			float *cur = &p->current->data[private->index].f;
	
			/* Save last sample, unfiltered */
			private->history[p->received % p->poolsize] = *cur;

			/* Reset accumulator */
			*cur = 0;
	
			/* FIR loop */
			for (int i = 0; i < MIN(ARRAY_LEN(coeffs), p->poolsize); i++)
				*cur += private->coeffs[i] * private->history[p->received+p->poolsize-i];
			break;
		}
	}

	return 0;
}

REGISTER_HOOK("decimate", 99, hook_decimate, HOOK_PRIVATE | HOOK_POST)
int hook_decimate(struct path *p, struct hook *h, int when)
{
	struct private {
		long ratio;
	} *private = h->private;
	
	switch (when) {
		case HOOK_INIT:
			if (!h->parameter)
				error("Missing parameter for hook 'decimate'");
		
			private = h->private = alloc(sizeof(struct private));
		
			private->ratio = strtol(h->parameter, NULL, 10);
			if (!private->ratio)
				error("Invalid parameter '%s' for hook 'decimate'", h->parameter);
			break;
			
		case HOOK_DEINIT:
			free(private);
			break;
			
		case HOOK_POST:
			return p->received % private->ratio;	
	}

	return 0;
}

REGISTER_HOOK("skip_first", 99, hook_skip_first, HOOK_PRIVATE | HOOK_POST | HOOK_PATH )
int hook_skip_first(struct path *p, struct hook *h, int when)
{
	struct private {
		double wait;			/**< Number of seconds to wait until first message is not skipped */
		struct timespec started;	/**< Timestamp of last simulation restart */
	} *private = h->private;
	
	switch (when) {
		case HOOK_INIT:
			if (!h->parameter)
				error("Missing parameter for hook 'skip_first'");

			private = h->private = alloc(sizeof(struct private));
		
			private->wait = strtof(h->parameter, NULL);
			if (!private->wait)
				error("Invalid parameter '%s' for hook 'skip_first'", h->parameter);
			break;
			
		case HOOK_DEINIT:
			free(private);
			break;

		case HOOK_PATH_RESTART:
			private->started = p->ts_recv;
			break;

		case HOOK_PATH_START:
			private->started = time_now();
			break;
			 
		case HOOK_POST: {
			double delta = time_delta(&private->started, &p->ts_recv);
			return delta < private->wait
				? -1 /* skip */
				: 0; /* send */
		}
	}
	
	return 0;
}

REGISTER_HOOK("restart", 1, hook_restart, HOOK_INTERNAL | HOOK_MSG)
int hook_restart(struct path *p, struct hook *h, int when)
{
	if (p->current->sequence  == 0 &&
	    p->previous->sequence <= UINT32_MAX - 32) {
		warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
			path_print(p), p->previous->sequence, p->current->sequence);

		p->sent	    =
		p->invalid  =
		p->skipped  =
		p->dropped  = 0;
		p->received = 1;

		if (path_run_hook(p, HOOK_PATH_RESTART))
			return -1;
	}

	return 0;
}

REGISTER_HOOK("verify", 2, hook_verify, HOOK_INTERNAL | HOOK_MSG)
int hook_verify(struct path *p, struct hook *h, int when)
{
	int reason = msg_verify(p->current);
	if (reason) {
		p->invalid++;
		warn("Received invalid message (reason = %d)", reason);
		return -1;
	}

	return 0;
}

REGISTER_HOOK("drop", 3, hook_drop, HOOK_INTERNAL | HOOK_MSG)
int hook_drop(struct path *p, struct hook *h, int when)
{
	int dist = p->current->sequence - (int32_t) p->previous->sequence;
	if (dist <= 0 && p->received > 1) {
		p->dropped++;
		return -1;
	}
	else
		return 0;
}

REGISTER_HOOK("stats", 2, hook_stats, HOOK_STATS)
int hook_stats(struct path *p, struct hook *h, int when)
{
	switch (when) {
		case HOOK_INIT:
			/** @todo Allow configurable bounds for histograms */
			hist_create(&p->hist_gap_seq, -HIST_SEQ, +HIST_SEQ, 1);
			hist_create(&p->hist_owd, 0, 1, 100e-3);
			hist_create(&p->hist_gap_msg,  90e-3, 110e-3, 1e-3);
			hist_create(&p->hist_gap_recv, 90e-3, 110e-3, 1e-3);
			break;
		
		case HOOK_DEINIT:
			hist_destroy(&p->hist_gap_seq);
			hist_destroy(&p->hist_owd);
			hist_destroy(&p->hist_gap_msg);
			hist_destroy(&p->hist_gap_recv);
			break;
			
		case HOOK_PRE:
			/* Exclude first message from statistics */
			if (p->received > 0) {
				double gap = time_delta(&p->ts_last, &p->ts_recv);
				
				hist_put(&p->hist_gap_recv, gap);
			}

		case HOOK_MSG: {
			struct msg *prev = p->previous, *cur = p->current;

			/* Exclude first message from statistics */
			if (p->received > 0) {			
				int dist     = cur->sequence - (int32_t) prev->sequence;
				double delay = time_delta(&MSG_TS(cur), &p->ts_recv);
				double gap   = time_delta(&MSG_TS(prev), &MSG_TS(cur));
				
				hist_put(&p->hist_gap_msg, gap);
				hist_put(&p->hist_gap_seq, dist);
				hist_put(&p->hist_owd, delay);
			}
			break;
		}
		
		case HOOK_PATH_STOP:
			if (p->hist_owd.total)     { info("One-way delay:"); hist_print(&p->hist_owd); }
			if (p->hist_gap_recv.total){ info("Inter-message arrival time:"); hist_print(&p->hist_gap_recv); }
			if (p->hist_gap_msg.total) { info("Inter-message ts gap:"); hist_print(&p->hist_gap_msg); }
			if (p->hist_gap_seq.total) { info("Inter-message sequence number gaps:"); hist_print(&p->hist_gap_seq); }
			break;

		case HOOK_PATH_RESTART:
			hist_reset(&p->hist_owd);
			hist_reset(&p->hist_gap_seq);
			hist_reset(&p->hist_gap_msg);
			hist_reset(&p->hist_gap_recv);
			break;
			
		case HOOK_PERIODIC: {
			char *buf = path_print(p);
			
			if (p->received > 1)
				stats("%-40.40s|%10.2g|%10.2f|%10u|%10u|%10u|%10u|%10u|%10u|%10u|", path_print(p),
					p->hist_owd.last, 1 / p->hist_gap_msg.last,
					p->sent, p->received, p->dropped, p->skipped, p->invalid, p->overrun, list_length(p->current)
				);
			else
				stats("%-40.40s|%10s|%10s|%10u|%10u|%10u|%10u|%10u|%10u|%10s|", buf, "", "", 
					p->sent, p->received, p->dropped, p->skipped, p->invalid, p->overrun, ""
				);
			
			free(buf);
			break;
		}
	}
	
	return 0;
}

void hook_stats_header()
{
#define UNIT(u)	"(" YEL(u) ")"

	stats("%-40s|%19s|%19s|%19s|%19s|%19s|%19s|%19s|%10s|%10s|", "Source " MAG("=>") " Destination",
		"OWD"	UNIT("S") " ",
		"Rate"	UNIT("p/S") " ",
		"Sent"	UNIT("p") " ",
		"Recv"	UNIT("p") " ",
		"Drop"	UNIT("p") " ",
		"Skip"	UNIT("p") " ",
		"Inval" UNIT("p") " ",
		"Overuns ",
		"Values "
	);
	line();
}

REGISTER_HOOK("stats_send", 99, hook_stats_send, HOOK_PRIVATE | HOOK_MSG)
int hook_stats_send(struct path *p, struct hook *h, int when)
{
	struct private {
		struct node *dest;
		int ratio;
	} *private = h->private;
	
	switch (when) {
		case HOOK_INIT:
			if (!h->parameter)
				error("Missing parameter for hook 'stats_send'");
			
			private = h->private = alloc(sizeof(struct private));
		
			private->dest = list_lookup(&nodes, h->parameter);
			if (!private->dest)
				error("Invalid destination node '%s' for hook 'stats_send'", h->parameter);
			break;

		case HOOK_DEINIT:
			free(private);
			break;

		case HOOK_MSG: {
			struct msg m = MSG_INIT(0);
			
			m.data[m.length++].f = p->sent;
			m.data[m.length++].f = p->received;
			m.data[m.length++].f = p->invalid;
			m.data[m.length++].f = p->skipped;
			m.data[m.length++].f = p->dropped;
			m.data[m.length++].f = p->hist_owd.last,
			m.data[m.length++].f = p->hist_gap_msg.last;
			m.data[m.length++].f = p->hist_gap_recv.last;
			
			/* Send single message with statistics to destination node */
			node_write(private->dest, &m, 1, 0, 1);
			
			break;
		}
	}
	
	return 0;
}