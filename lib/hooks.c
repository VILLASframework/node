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
#include "node.h"

struct list hooks;

/* Those references can be used inside the hook callbacks after initializing them with hook_init() */
static struct list *hook_nodes = NULL;
static struct list *hook_paths = NULL;
static struct settings *hook_settings = NULL;

void hook_init(struct list *nodes, struct list *paths, struct settings *set)
{
	hook_nodes = nodes;
	hook_paths = paths;
	hook_settings = set;
}

int hooks_sort_priority(const void *a, const void *b) {
	struct hook *ha = (struct hook *) a;
	struct hook *hb = (struct hook *) b;
	
	return ha->priority - hb->priority;
}

REGISTER_HOOK("print", 99, hook_print, HOOK_MSG)
int hook_print(struct path *p, struct hook *h, int when)
{
	struct msg *m = pool_current(&p->pool);
	double offset = time_delta(&MSG_TS(m), &p->ts.recv);
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
	struct msg *m = pool_current(&p->pool);

	m->ts.sec = p->ts.recv.tv_sec;
	m->ts.nsec = p->ts.recv.tv_nsec;

	return 0;
}

REGISTER_HOOK("fix_ts", 0, hook_fix_ts, HOOK_INTERNAL | HOOK_MSG)
int hook_fix_ts(struct path *p, struct hook *h, int when)
{
	struct msg *m = pool_current(&p->pool);

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
	} *x = h->private;	
	
	switch (when) {
		case HOOK_PATH_START:
			x = h->private = alloc(sizeof(struct private));
		
			if (!h->parameter)
				error("Missing parameter for hook 'deduplication'");

			x->threshold = strtof(h->parameter, NULL);
			if (!x->threshold)
				error("Failed to parse parameter '%s' for hook 'deduplication'", h->parameter);
			break;
			
		case HOOK_PATH_STOP:
			free(x);
			break;
			
		case HOOK_ASYNC: {
			int ret = 0;

			struct msg *prev = &x->previous;
			struct msg *cur = pool_current(&p->pool);
	
			for (int i = 0; i < MIN(cur->values, prev->values); i++) {
				if (fabs(cur->data[i].f - prev->data[i].f) > x->threshold)
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
	} *x = h->private;
	
	switch (when) {
		case HOOK_PATH_START:
			x = h->private = alloc(sizeof(struct private));
		
			if (!h->parameter)
				error("Missing parameter for hook 'deduplication'");
			
			if      (!strcmp(h->parameter, "fixed"))
				x->mode = TO_FIXED;
			else if (!strcmp(h->parameter, "float"))
				x->mode = TO_FLOAT;
			else
				error("Invalid parameter '%s' for hook 'convert'", h->parameter);
			break;
			
		case HOOK_PATH_STOP:
			free(x);
			break;
			
		case HOOK_MSG: {
			struct msg *m = pool_current(&p->pool);

			for (int i = 0; i < m->values; i++) {
				switch (x->mode) {
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
	char *end;

	struct private {
		struct pool coeffs;
		struct pool history;
		int index;
	} *x = h->private;
	
	switch (when) {
		case HOOK_PATH_START:
			if (!h->parameter)
				error("Missing parameter for hook 'fir'");

			x = h->private = alloc(sizeof(struct private));
		
			pool_create(&x->coeffs,  ARRAY_LEN(coeffs), sizeof(double));
			pool_create(&x->history, ARRAY_LEN(coeffs), sizeof(double));

			/** Fill with static coefficients */
			memcpy(x->coeffs.buffer, coeffs, sizeof(coeffs));

			x->index = strtol(h->parameter, &end, 10);
			if (h->parameter == end)
				error("Invalid parameter '%s' for hook 'fir'", h->parameter);
			break;

		case HOOK_PATH_STOP:
			pool_destroy(&x->coeffs);
			pool_destroy(&x->history);

			free(x);
			break;

		case HOOK_MSG: {
			/* Current value of interest */
			struct msg *m = pool_current(&p->pool);
			float  *value = &m->data[x->index].f;
			double *history = pool_current(&x->history);
	
			/* Save last sample, unfiltered */
			*history = *value;

			/* Reset accumulator */
			*value = 0;
	
			/* FIR loop */
			for (int i = 0; i < pool_length(&x->coeffs); i++) {
				double *coeff = pool_get(&x->coeffs, i);
				double *hist  = pool_getrel(&x->history, -i);
				
				*value += *coeff * *hist;
			}
	
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
	} *x = h->private;
	
	switch (when) {
		case HOOK_PATH_START:
			if (!h->parameter)
				error("Missing parameter for hook 'decimate'");
		
			x = h->private = alloc(sizeof(struct private));
		
			x->ratio = strtol(h->parameter, NULL, 10);
			if (!x->ratio)
				error("Invalid parameter '%s' for hook 'decimate'", h->parameter);
			break;
			
		case HOOK_PATH_STOP:
			free(x);
			break;
			
		case HOOK_POST:
			return p->received % x->ratio;	
	}

	return 0;
}

REGISTER_HOOK("skip_first", 99, hook_skip_first, HOOK_PRIVATE | HOOK_POST | HOOK_PATH )
int hook_skip_first(struct path *p, struct hook *h, int when)
{
	struct private {
		double wait;			/**< Number of seconds to wait until first message is not skipped */
		struct timespec started;	/**< Timestamp of last simulation restart */
	} *x = h->private;
	
	switch (when) {
		case HOOK_PATH_START:
			if (!h->parameter)
				error("Missing parameter for hook 'skip_first'");

			x = h->private = alloc(sizeof(struct private));
		
			x->started = time_now();
			x->wait = strtof(h->parameter, NULL);
			if (!x->wait)
				error("Invalid parameter '%s' for hook 'skip_first'", h->parameter);
			break;
			
		case HOOK_PATH_STOP:
			free(x);
			break;

		case HOOK_PATH_RESTART:
			x->started = p->ts.recv;
			break;

		case HOOK_POST: {
			double delta = time_delta(&x->started, &p->ts.recv);
			return delta < x->wait
				? -1 /* skip */
				: 0; /* send */
		}
	}
	
	return 0;
}

REGISTER_HOOK("restart", 1, hook_restart, HOOK_INTERNAL | HOOK_MSG)
int hook_restart(struct path *p, struct hook *h, int when)
{
	struct msg *cur  = pool_current(&p->pool);
	struct msg *prev = pool_previous(&p->pool);

	if (cur->sequence  == 0 &&
	    prev->sequence <= UINT32_MAX - 32) {
		warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u)",
			path_name(p), prev->sequence, cur->sequence);

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
	struct msg *cur  = pool_current(&p->pool);

	int reason = msg_verify(cur);
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
	struct msg *cur  = pool_current(&p->pool);
	struct msg *prev = pool_previous(&p->pool);
	
	int dist = cur->sequence - (int32_t) prev->sequence;
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
		case HOOK_PATH_START:
			/** @todo Allow configurable bounds for histograms */
			hist_create(&p->hist.owd, 0, 1, 100e-3);
			hist_create(&p->hist.gap_msg,  90e-3, 110e-3, 1e-3);
			hist_create(&p->hist.gap_recv, 90e-3, 110e-3, 1e-3);
			hist_create(&p->hist.gap_seq, -HIST_SEQ, +HIST_SEQ, 1);
			break;
		
		case HOOK_PRE:
			/* Exclude first message from statistics */
			if (p->received > 0) {
				double gap = time_delta(&p->ts.last, &p->ts.recv);
				
				hist_put(&p->hist.gap_recv, gap);
			}

		case HOOK_MSG: {
			struct msg *cur  = pool_current(&p->pool);
			struct msg *prev = pool_previous(&p->pool);

			/* Exclude first message from statistics */
			if (p->received > 0) {			
				int dist     = cur->sequence - (int32_t) prev->sequence;
				double delay = time_delta(&MSG_TS(cur), &p->ts.recv);
				double gap   = time_delta(&MSG_TS(prev), &MSG_TS(cur));
				
				hist_put(&p->hist.gap_msg, gap);
				hist_put(&p->hist.gap_seq, dist);
				hist_put(&p->hist.owd, delay);
			}
			break;
		}
		
		case HOOK_PATH_STOP:
			if (p->hist.owd.total)     { info("One-way delay:"); hist_print(&p->hist.owd); }
			if (p->hist.gap_recv.total){ info("Inter-message arrival time:"); hist_print(&p->hist.gap_recv); }
			if (p->hist.gap_msg.total) { info("Inter-message ts gap:"); hist_print(&p->hist.gap_msg); }
			if (p->hist.gap_seq.total) { info("Inter-message sequence number gaps:"); hist_print(&p->hist.gap_seq); }
			
			hist_destroy(&p->hist.owd);
			hist_destroy(&p->hist.gap_msg);
			hist_destroy(&p->hist.gap_recv);
			hist_destroy(&p->hist.gap_seq);
			break;

		case HOOK_PATH_RESTART:
			hist_reset(&p->hist.owd);
			hist_reset(&p->hist.gap_seq);
			hist_reset(&p->hist.gap_msg);
			hist_reset(&p->hist.gap_recv);
			break;
			
		case HOOK_PERIODIC: {
			if (p->received > 1) {
				struct msg *cur = pool_current(&p->pool);

				stats("%-40.40s|%10.2g|%10.2f|%10u|%10u|%10u|%10u|%10u|%10u|%10u|", path_name(p),
					p->hist.owd.last, 1 / p->hist.gap_msg.last,
					p->sent, p->received, p->dropped, p->skipped, p->invalid, p->overrun, cur->values
				);
			}
			else
				stats("%-40.40s|%10s|%10s|%10u|%10u|%10u|%10u|%10u|%10u|%10s|", path_name(p), "", "", 
					p->sent, p->received, p->dropped, p->skipped, p->invalid, p->overrun, ""
				);
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

/** @todo Come up with a better solution for this ugly workaround */
struct list *hook_nodes = NULL;

REGISTER_HOOK("stats_send", 99, hook_stats_send, HOOK_PRIVATE | HOOK_PERIODIC)
int hook_stats_send(struct path *p, struct hook *h, int when)
{
	struct private {
		struct node *dest;
		struct msg *msg;
		int ratio;
	} *x = h->private;
	
	switch (when) {
		case HOOK_PATH_START:
			if (!h->parameter)
				error("Missing parameter for hook 'stats_send'");
			
			if (!hook_nodes)
				error("stats_send() hook has no reference to node list");
			
			x = h->private = alloc(sizeof(struct private));
		
			x->msg = msg_create(9);
			x->dest = list_lookup(hook_nodes, h->parameter);
			if (!x->dest)
				error("Invalid destination node '%s' for hook 'stats_send'", h->parameter);
			break;

		case HOOK_PATH_STOP:
			free(x->msg);
			free(x);
			break;

		case HOOK_PERIODIC:	
			x->msg->data[0].f = p->sent;
			x->msg->data[1].f = p->received;
			x->msg->data[2].f = p->invalid;
			x->msg->data[3].f = p->skipped;
			x->msg->data[4].f = p->dropped;
			x->msg->data[5].f = p->overrun;
			x->msg->data[6].f = p->hist.owd.last,
			x->msg->data[7].f = 1.0 / p->hist.gap_msg.last;
			x->msg->data[8].f = 1.0 / p->hist.gap_recv.last;
			
			node_write_single(x->dest, x->msg); /* Send single message with statistics to destination node */
			break;
	}
	
	return 0;
}