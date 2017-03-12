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
#include "utils.h"
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
	
	s->delta = alloc(sizeof(struct stats_delta));

	return 0;
}

void stats_destroy(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		hist_destroy(&s->histograms[i]);
	}
	
	free(s->delta);
}

void stats_update(struct stats_delta *d, enum stats_id id, double val)
{
	assert(id >= 0 && id < STATS_COUNT);
		
	d->values[id] = val;
	d->update |= 1 << id;
}

int stats_commit(struct stats *s, struct stats_delta *d)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		if (d->update & 1 << i)
			hist_put(&s->histograms[i], d->values[i]);
	}
	
	return 0;
}

void stats_collect(struct stats_delta *s, struct sample *smps[], size_t cnt)
{
	struct sample *previous = s->last;
	
	for (int i = 0; i < cnt; i++) {
		if (previous) {
			stats_update(s, STATS_GAP_RECEIVED, time_delta(&previous->ts.received, &smps[i]->ts.received));
			stats_update(s, STATS_GAP_SAMPLE,   time_delta(&previous->ts.origin,   &smps[i]->ts.origin));
			stats_update(s, STATS_OWD,          time_delta(&smps[i]->ts.origin,    &smps[i]->ts.received));
			stats_update(s, STATS_GAP_SEQUENCE, smps[i]->sequence - (int32_t) previous->sequence);
		}
	
		previous = smps[i];
	}

	if (s->last)
		sample_put(s->last);

	if (previous)
		sample_get(previous);

	s->last = previous;
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