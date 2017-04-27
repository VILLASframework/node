/** Statistic collection.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include "stats.h"
#include "hist.h"
#include "timing.h"
#include "path.h"
#include "sample.h"
#include "utils.h"
#include "log.h"
#include "node.h"

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
	{ "skipped",	  "samples",	"skipped samples by hooks",						{0,	0,	-1,	}},
	{ "reorderd",	  "samples",	"reordered samples",							{0,	20,	1,	}},
	{ "gap_sequence", "samples",	"sequence number displacement of received messages",			{-10,	10, 20,		}},
	{ "gap_sample",	  "seconds",	"inter message timestamps (as sent by remote)",				{90e-3,	110e-3,	1e-3,	}},
	{ "gap_received", "seconds",	"Histogram for inter message arrival time (as seen by this instance)",	{90e-3,	110e-3,	1e-3,	}},
	{ "owd",	  "seconds",	"Histogram for one-way-delay (OWD) of received messages",		{0,	1,	100e-3,	}}
};

int stats_init(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_table[i];
		hist_init(&s->histograms[i], desc->hist.min, desc->hist.max, desc->hist.resolution);
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
		if (d->update & 1 << i) {
			hist_put(&s->histograms[i], d->values[i]);
			d->update &= ~(1 << i);
		}
	}
	
	return 0;
}

void stats_collect(struct stats_delta *s, struct sample *smps[], size_t cnt)
{
	int dist;
	struct sample *previous = s->last;
	
	for (int i = 0; i < cnt; i++) {
		if (previous) {
			stats_update(s, STATS_GAP_RECEIVED, time_delta(&previous->ts.received, &smps[i]->ts.received));
			stats_update(s, STATS_GAP_SAMPLE,   time_delta(&previous->ts.origin,   &smps[i]->ts.origin));
			stats_update(s, STATS_OWD,          -time_delta(&smps[i]->ts.origin,    &smps[i]->ts.received));
			stats_update(s, STATS_GAP_SEQUENCE, smps[i]->sequence - (int32_t) previous->sequence);
			
			dist = smps[i]->sequence - (int32_t) previous->sequence;
			if (dist > 0)
				stats_update(s, STATS_REORDERED,    dist);
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

json_t * stats_json_periodic(struct stats *s, struct path *p)
{
	return json_pack("{ s: s, s: f, s: f, s: i, s: i }"
		"path", path_name(p),
		"owd", s->histograms[STATS_OWD].last,
		"rate", 1.0 / s->histograms[STATS_GAP_SAMPLE].last,
		"dropped", s->histograms[STATS_REORDERED].total,
		"skipped", s->histograms[STATS_SKIPPED].total
	);
}
#endif

void stats_reset(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		hist_reset(&s->histograms[i]);
	}
}

void stats_print_header(enum stats_format fmt)
{
	#define UNIT(u)	"(" YEL(u) ")"
	
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			stats("%-40s|%19s|%19s|%19s|%19s|", "Source " MAG("=>") " Destination",
				"OWD"	UNIT("S") " ",
				"Rate"	UNIT("p/S") " ",
				"Drop"	UNIT("p") " ",
				"Skip"	UNIT("p") " "
			);
			line();
			break;
			
		default: { }
	}
}

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, int verbose, struct path *p)
{
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			stats("%-40.40s|%10f|%10f|%10ju|%10ju|", path_name(p),
				s->histograms[STATS_OWD].last,
				1.0 / s->histograms[STATS_GAP_SAMPLE].last,
				s->histograms[STATS_REORDERED].total,
				s->histograms[STATS_SKIPPED].total
			);
			break;
		
		case STATS_FORMAT_JSON: {
			json_t *json_stats = stats_json_periodic(s, p);
			json_dumpf(json_stats, f, 0);
			break;
		}
			
		default: { }
	}
}

void stats_print(struct stats *s, FILE *f, enum stats_format fmt, int verbose)
{
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			for (int i = 0; i < STATS_COUNT; i++) {
				struct stats_desc *desc = &stats_table[i];
		
				stats("%s: %s", desc->name, desc->desc);
				hist_print(&s->histograms[i], verbose);
			}
			break;
			
		case STATS_FORMAT_JSON: {
			json_t *json_stats = stats_json(s);
			json_dumpf(json_stats, f, 0);
			fflush(f);
			break;
		}
			
		default: { }
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

enum stats_id stats_lookup_id(const char *name)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_table[i];
		
		if (!strcmp(desc->name, name))
			return i;
	}
	
	return -1;
}