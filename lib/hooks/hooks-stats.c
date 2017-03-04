/** Statistic-related hook functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include "hooks.h"
#include "sample.h"
#include "path.h"
#include "utils.h"
#include "timing.h"

extern struct list *hook_nodes;

void hook_stats_header()
{
	#define UNIT(u)	"(" YEL(u) ")"

	stats("%-40s|%19s|%19s|%19s|%19s|%19s|%10s|", "Source " MAG("=>") " Destination",
		"OWD"	UNIT("S") " ",
		"Rate"	UNIT("p/S") " ",
		"Drop"	UNIT("p") " ",
		"Skip"	UNIT("p") " ",
		"Inval" UNIT("p") " ",
		"Overuns "
	);
	line();
}

REGISTER_HOOK("stats", "Collect statistics for the current path", 2, 1, hook_stats, HOOK_STATS)
int hook_stats(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	switch (when) {
		case HOOK_INIT:
			/** @todo Allow configurable bounds for histograms */
			hist_create(&p->hist.owd,      0,         1,         100e-3);
			hist_create(&p->hist.gap_msg,  90e-3,     110e-3,      1e-3);
			hist_create(&p->hist.gap_recv, 90e-3,     110e-3,      1e-3);
			hist_create(&p->hist.gap_seq,  -HIST_SEQ, +HIST_SEQ,   1);
			break;

		case HOOK_READ:
			for (int i = 0; i < cnt; i++) {
				h->last  = smps[i];
				
				if (h->prev) {
					int    gap_seq  = h->last->sequence - (int32_t) h->prev->sequence;
					double owd      = time_delta(&h->last->ts.origin, &h->last->ts.received);
					double gap      = time_delta(&h->prev->ts.origin, &h->last->ts.origin);
					double gap_recv = time_delta(&h->prev->ts.received, &h->last->ts.received);

					hist_put(&p->hist.gap_msg, gap);
					hist_put(&p->hist.gap_seq, gap_seq);
					hist_put(&p->hist.owd, owd);
					hist_put(&p->hist.gap_recv, gap_recv);
				}

				h->prev = h->last;
			}
			break;

		case HOOK_PATH_STOP:
			if (p->hist.owd.total)     { info("One-way delay:"); hist_print(&p->hist.owd); }
			if (p->hist.gap_recv.total){ info("Inter-message arrival time:"); hist_print(&p->hist.gap_recv); }
			if (p->hist.gap_msg.total) { info("Inter-message ts gap:"); hist_print(&p->hist.gap_msg); }
			if (p->hist.gap_seq.total) { info("Inter-message sequence number gaps:"); hist_print(&p->hist.gap_seq); }
			break;
		
		case HOOK_DEINIT:
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
			
		case HOOK_PERIODIC:
			stats("%-40.40s|%10s|%10s|%10ju|%10ju|%10ju|%10ju|", path_name(p), "", "", 
				p->dropped, p->skipped, p->invalid, p->overrun);
			break;
	}
	
	return cnt;
}

REGISTER_HOOK("stats_send", "Send path statistics to another node", 99, 0, hook_stats_send, HOOK_STORAGE | HOOK_PARSE | HOOK_PERIODIC | HOOK_PATH)
int hook_stats_send(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	struct private {
		struct node *dest;
		int ratio;
	} *private = hook_storage(h, when, sizeof(*private));
	
	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook '%s'", h->name);
			
			if (!hook_nodes)
				error("Missing reference to node list for hook '%s", h->name);

			private->dest = list_lookup(hook_nodes, h->parameter);
			if (!private->dest)
				error("Invalid destination node '%s' for hook '%s'", h->parameter, h->name);

			node_start(private->dest);
			
			break;

		case HOOK_PERIODIC: {
			int i;
			char buf[SAMPLE_LEN(16)];
			struct sample *smp = (struct sample *) buf;
			
			i = 0;
			smp->data[i++].f = p->invalid;
			smp->data[i++].f = p->skipped;
			smp->data[i++].f = p->dropped;
			smp->data[i++].f = p->overrun;
			smp->data[i++].f =       p->hist.owd.last,
			smp->data[i++].f = 1.0 / p->hist.gap_msg.last;
			smp->data[i++].f = 1.0 / p->hist.gap_recv.last;
			smp->length = i;
			
			node_write(private->dest, &smp, 1); /* Send single message with statistics to destination node */
			break;
		}
	}
	
	return 0;
}