/** Statistic collection.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include "stats.h"
#include "hist.h"
#include "timing.h"
#include "path.h"
#include "sample.h"
#include "log.h"

int stats_init(struct stats *s)
{
	/** @todo Allow configurable bounds for histograms */
	hist_create(&s->histogram.owd,      0,         1,         100e-3);
	hist_create(&s->histogram.gap_msg,  90e-3,     110e-3,      1e-3);
	hist_create(&s->histogram.gap_recv, 90e-3,     110e-3,      1e-3);
	hist_create(&s->histogram.gap_seq,  -HIST_SEQ, +HIST_SEQ,   1);
	
	return 0;
}

void stats_destroy(struct stats *s)
{
	hist_destroy(&s->histogram.owd);
	hist_destroy(&s->histogram.gap_msg);
	hist_destroy(&s->histogram.gap_recv);
	hist_destroy(&s->histogram.gap_seq);
}

void stats_collect(struct stats *s, struct sample *smps[], size_t cnt)
{
	for (int i = 0; i < cnt; i++) {
		if (s->last) {
			int    gap_seq  = smps[i]->sequence - (int32_t) s->last->sequence;
			double owd      = time_delta(&smps[i]->ts.origin,   &smps[i]->ts.received);
			double gap      = time_delta(&s->last->ts.origin,   &smps[i]->ts.origin);
			double gap_recv = time_delta(&s->last->ts.received, &smps[i]->ts.received);

			hist_put(&s->histogram.owd,      owd);
			hist_put(&s->histogram.gap_msg,  gap);
			hist_put(&s->histogram.gap_seq,  gap_seq);
			hist_put(&s->histogram.gap_recv, gap_recv);
		}

		if (i == 0 && s->last)
			sample_put(s->last);
		if (i == cnt - 1)
			sample_get(smps[i]);

		s->last = smps[i];
	}
}

#ifdef WITH_JANSSON
json_t * stats_json(struct stats *s)
{
	return json_pack("{ s: { s: i, s: i, s: i }, s: { s: o, s: o, s: o } }",
			"counter",
				"dropped", s->counter.dropped,
				"invalid", s->counter.invalid,
				"skipped", s->counter.skipped,
			"histogram",
				"owd",     hist_json(&s->histogram.owd),
				"gap_msg", hist_json(&s->histogram.gap_msg),
				"gap_recv",hist_json(&s->histogram.gap_recv),
				"gap_seq", hist_json(&s->histogram.gap_seq)
	);
}
#endif

void stats_reset(struct stats *s)
{
	s->counter.invalid  =
	s->counter.skipped  =
	s->counter.dropped  = 0;

	hist_reset(&s->histogram.owd);
	hist_reset(&s->histogram.gap_seq);
	hist_reset(&s->histogram.gap_msg);
	hist_reset(&s->histogram.gap_recv);
}

void stats_print_header()
{
	#define UNIT(u)	"(" YEL(u) ")"

	stats("%-40s|%19s|%19s|%19s|%19s|%19s|", "Source " MAG("=>") " Destination",
		"OWD"	UNIT("S") " ",
		"Rate"	UNIT("p/S") " ",
		"Drop"	UNIT("p") " ",
		"Skip"	UNIT("p") " ",
		"Inval" UNIT("p") " "
	);
	line();
}

void stats_print_periodic(struct stats *s, struct path *p)
{
	stats("%-40.40s|%10s|%10s|%10ju|%10ju|%10ju|", path_name(p), "", "", 
		s->counter.dropped, s->counter.skipped, s->counter.invalid);
}

void stats_print(struct stats *s)
{
	stats("Dropped samples: %ju", s->counter.dropped);
	stats("Skipped samples: %ju", s->counter.skipped);
	stats("Invalid samples: %ju", s->counter.invalid);

	stats("One-way delay:");	
	hist_print(&s->histogram.owd);
	
	stats("Inter-message arrival time:");
	hist_print(&s->histogram.gap_recv);
	
	stats("Inter-message ts gap:");
	hist_print(&s->histogram.gap_msg);
	
	stats("Inter-message sequence number gaps:");
	hist_print(&s->histogram.gap_seq);
}

void stats_send(struct stats *s, struct node *n)
{
	char buf[SAMPLE_LEN(16)];
	struct sample *smp = (struct sample *) buf;
	
	int i = 0;
	smp->data[i++].f = s->counter.invalid; /**< Use integer here? */
	smp->data[i++].f = s->counter.skipped;
	smp->data[i++].f = s->counter.dropped;
	smp->data[i++].f =       s->histogram.owd.last,
	smp->data[i++].f = 1.0 / s->histogram.gap_msg.last;
	smp->data[i++].f = 1.0 / s->histogram.gap_recv.last;
	smp->length = i;
	
	node_write(n, &smp, 1); /* Send single message with statistics to destination node */
}