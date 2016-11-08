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

static struct stats_desc {
	const char *name;
	const char *unit;
	const char *desc;
	struct {
		double min;
		double max;
		double resolution;
	} hist;
} stats_table[] = {
	{ "skipped",	  "",	"skipped messages by hooks",						{0,	0,	-1,	}},
	{ "dropped",	  "",	"dropped messages because of reordering",				{0,	0,	-1,	}},
	{ "gap_sequence", "",	"sequence number displacement of received messages",			{-10,	10, 20,		}},
	{ "gap_sample",	  "",	"inter message timestamps (as sent by remote)",				{90e-3,	110e-3,	1e-3,	}},
	{ "gap_received", "",	"Histogram for inter message arrival time (as seen by this instance)",	{90e-3,	110e-3,	1e-3,	}},
	{ "owd",	  "s",	"Histogram for one-way-delay (OWD) of received messages",		{0,	1,	100e-3,	}}
};

int stats_init(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_table[i];
		hist_create(&s->histograms[i], desc->hist.min, desc->hist.max, desc->hist.resolution);
	}

	return 0;
}

void stats_destroy(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		hist_destroy(&s->histograms[i]);
	}
}

void stats_update(struct stats *s, enum stats_id id, double val)
{
	if (!s)
		return;
	
	hist_put(&s->histograms[id], val);
}

#if 0
int stats_delta(struct stats_delta *d, struct sample *s, struct sample *p)
{
	d->histogram.owd      = time_delta(&smps[i]->ts.origin,   &smps[i]->ts.received);
	d->histogram.gap      = time_delta(&s->last->ts.origin,   &smps[i]->ts.origin);
	d->histogram.gap_seq  = s->sequence - (int32_t) p->sequence;
	d->histogram.gap_recv = time_delta(&s->last->ts.received, &smps[i]->ts.received);
	
	d->counter.dropped    = d->histogram.gap_seq <= 0 ? 1 : 0;
	d->counter.invalid    = 0;
	
	return 0;
}
#endif

int stats_commit(struct stats *s, struct stats_delta *d)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		hist_put(&s->histograms[i], d->vals[i]);
	}
	
	return 0;
}

void stats_collect(struct stats *s, struct sample *smps[], size_t cnt)
{
	for (int i = 0; i < cnt; i++) {
		if (s->last) {
//			struct stats_delta d;
//			stats_get_delta(&d, smps[i], s->last);
//			stats_commit(s, &d);
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
	json_t *obj = json_object();

	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_table[i];
		
		json_t *stats = hist_json(&s->histograms[i]);
		
		json_object_set(obj, desc->name, stats);
	}
	
	return obj;
}
#endif

void stats_reset(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		hist_reset(&s->histograms[i]);
	}
}

void stats_print_header()
{
	#define UNIT(u)	"(" YEL(u) ")"

	stats("%-40s|%19s|%19s|%19s|%19s|", "Source " MAG("=>") " Destination",
		"OWD"	UNIT("S") " ",
		"Rate"	UNIT("p/S") " ",
		"Drop"	UNIT("p") " ",
		"Skip"	UNIT("p") " "
	);
	line();
}

void stats_print_periodic(struct stats *s, struct path *p)
{
	stats("%-40.40s|%10f|%10f|%10ju|%10ju|", path_name(p),
		s->histograms[STATS_OWD].last,
		1.0 / s->histograms[STATS_GAP_SAMPLE].last,
		s->histograms[STATS_DROPPED].total,
		s->histograms[STATS_SKIPPED].total
	);
}

void stats_print(struct stats *s, int details)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_table[i];
		
		stats("%s: %s", desc->name, desc->desc);
		hist_print(&s->histograms[i], details);
	}
}

void stats_send(struct stats *s, struct node *n)
{
	char buf[SAMPLE_LEN(STATS_COUNT * 5)];
	struct sample *smp = (struct sample *) buf;
	
	int i = 0;
	
	for (int j = 0; j < STATS_COUNT; j++) {
		smp->data[i++].f = s->histograms[j].last;
		smp->data[i++].f = s->histograms[j].highest;
		smp->data[i++].f = s->histograms[j].lowest;
		smp->data[i++].f = hist_mean(&s->histograms[j]);
		smp->data[i++].f = hist_var(&s->histograms[j]);
	}
	smp->length = i;
	
	node_write(n, &smp, 1); /* Send single message with statistics to destination node */
}