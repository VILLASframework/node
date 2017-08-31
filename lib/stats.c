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

#include <string.h>

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
	int hist_buckets;
} stats_metrics[] = {
	{ "skipped",	  "samples",	"Skipped samples and the distance between them",		25 },
	{ "reordered",	  "samples",	"Reordered samples and the distance between them",		25 },
	{ "gap_sample",	  "seconds",	"Inter-message timestamps (as sent by remote)",			25 },
	{ "gap_received", "seconds",	"Inter-message arrival time (as seen by this instance)",	25 },
	{ "owd",	  "seconds",	"One-way-delay (OWD) of received messages",			25 }
};

int stats_lookup_format(const char *str)
{
	if      (!strcmp(str, "human"))
		return STATS_FORMAT_HUMAN;
	else if (!strcmp(str, "json"))
		return STATS_FORMAT_JSON;
	else if (!strcmp(str, "matlab"))
	 	return STATS_FORMAT_MATLAB;
	else
		return -1;
}

int stats_init(struct stats *s, int buckets, int warmup)
{
	for (int i = 0; i < STATS_COUNT; i++)
		hist_init(&s->histograms[i], buckets, warmup);

	s->delta = alloc(sizeof(struct stats_delta));

	return 0;
}

int stats_destroy(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++)
		hist_destroy(&s->histograms[i]);

	free(s->delta);

	return 0;
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
			stats_update(s, STATS_OWD,          time_delta(&smps[i]->ts.origin,    &smps[i]->ts.received));

			dist = smps[i]->sequence - (int32_t) previous->sequence;
			if (dist != 1)
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

json_t * stats_json(struct stats *s)
{
	json_t *obj = json_object();

	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_metrics[i];

		json_t *stats = hist_json(&s->histograms[i]);

		json_object_set(obj, desc->name, stats);
	}

	return obj;
}

json_t * stats_json_periodic(struct stats *s, struct path *p)
{
	return json_pack("{ s: s, s: f, s: f, s: i, s: i }"
		"path", path_name(p),
		"owd", hist_last(&s->histograms[STATS_OWD]),
		"rate", 1.0 / hist_last(&s->histograms[STATS_GAP_SAMPLE]),
		"dropped", hist_total(&s->histograms[STATS_REORDERED]),
		"skipped", hist_total(&s->histograms[STATS_SKIPPED])
	);
}

void stats_reset(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		hist_reset(&s->histograms[i]);
	}
}

static struct table_column stats_cols[] = {
	{ 35, "Path", "%s", NULL,   TABLE_ALIGN_LEFT },
	{ 10, "Cnt",  "%ju", "p",   TABLE_ALIGN_RIGHT },
	{ 10, "OWD last",  "%f",  "S",   TABLE_ALIGN_RIGHT },
	{ 10, "OWD mean",  "%f",  "S",   TABLE_ALIGN_RIGHT },
	{ 10, "Rate last", "%f",  "p/S", TABLE_ALIGN_RIGHT },
	{ 10, "Rate mean", "%f",  "p/S", TABLE_ALIGN_RIGHT },
	{ 10, "Drop", "%ju", "p",   TABLE_ALIGN_RIGHT },
	{ 10, "Skip", "%ju", "p",   TABLE_ALIGN_RIGHT }
};

static struct table stats_table = {
	.ncols = ARRAY_LEN(stats_cols),
	.cols = stats_cols
};

void stats_print_header(enum stats_format fmt)
{
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			table_header(&stats_table);
			break;

		default: { }
	}
}

void stats_print_footer(enum stats_format fmt)
{
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			table_footer(&stats_table);
			break;

		default: { }
	}
}

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, int verbose, struct path *p)
{
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			table_row(&stats_table,
				path_name(p),
				hist_total(&s->histograms[STATS_OWD]),
				hist_last(&s->histograms[STATS_OWD]),
				hist_mean(&s->histograms[STATS_OWD]),
				1.0 / hist_last(&s->histograms[STATS_GAP_RECEIVED]),
				1.0 / hist_mean(&s->histograms[STATS_GAP_RECEIVED]),
				hist_total(&s->histograms[STATS_REORDERED]),
				hist_total(&s->histograms[STATS_SKIPPED])
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
				struct stats_desc *desc = &stats_metrics[i];

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
		smp->data[i++].f = hist_last(&s->histograms[j]);
		smp->data[i++].f = hist_highest(&s->histograms[j]);
		smp->data[i++].f = hist_lowest(&s->histograms[j]);
		smp->data[i++].f = hist_mean(&s->histograms[j]);
		smp->data[i++].f = hist_var(&s->histograms[j]);
	}
	smp->length = i;

	node_write(n, &smp, 1); /* Send single message with statistics to destination node */
}

enum stats_id stats_lookup_id(const char *name)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_metrics[i];

		if (!strcmp(desc->name, name))
			return i;
	}

	return -1;
}
