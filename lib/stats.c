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

#include <villas/stats.h>
#include <villas/hist.h>
#include <villas/timing.h>
#include <villas/node.h>
#include <villas/sample.h>
#include <villas/utils.h>
#include <villas/log.h>
#include <villas/node.h>
#include <villas/table.h>

static struct stats_desc {
	const char *name;
	const char *unit;
	const char *desc;
	int hist_buckets;
} stats_metrics[] = {
	{ "skipped",	  "samples", "Skipped samples and the distance between them",		25 },
	{ "time",	  "seconds", "The processing time per sample within VILLAsnode",	25 },
	{ "reordered",	  "samples", "Reordered samples and the distance between them",		25 },
	{ "gap_sample",	  "seconds", "Inter-message timestamps (as sent by remote)",		25 },
	{ "gap_received", "seconds", "Inter-message arrival time (as seen by this instance)",	25 },
	{ "owd",	  "seconds", "One-way-delay (OWD) of received messages",		25 }
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

void stats_update(struct stats *s, enum stats_id id, double val)
{
	s->delta->values[id] = val;
	s->delta->update |= 1 << id;
}

int stats_commit(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		if (s->delta->update & 1 << i) {
			hist_put(&s->histograms[i], s->delta->values[i]);
			s->delta->update &= ~(1 << i);
		}
	}

	return 0;
}

json_t * stats_json(struct stats *s)
{
	json_t *obj = json_object();

	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *d = &stats_metrics[i];
		struct hist *h = &s->histograms[i];

		json_object_set_new(obj, d->name, hist_json(h));
	}

	return obj;
}

json_t * stats_json_periodic(struct stats *s, struct node *n)
{
	return json_pack("{ s: s, s: i, s: i, s: f, s: f, s: i, s: i }",
		"node", node_name(n),
		"received", hist_total(&s->histograms[STATS_OWD]),
		"sent", hist_total(&s->histograms[STATS_TIME]),
		"owd", hist_last(&s->histograms[STATS_OWD]),
		"rate", 1.0 / hist_last(&s->histograms[STATS_GAP_SAMPLE]),
		"dropped", hist_total(&s->histograms[STATS_REORDERED]),
		"skipped", hist_total(&s->histograms[STATS_SKIPPED])
	);
}

void stats_reset(struct stats *s)
{
	for (int i = 0; i < STATS_COUNT; i++)
		hist_reset(&s->histograms[i]);
}

static struct table_column stats_cols[] = {
	{ 10, "Node",		"%s",	NULL,		TABLE_ALIGN_LEFT },
	{ 10, "Recv",		"%ju",	"pkts",		TABLE_ALIGN_RIGHT },
	{ 10, "Sent",		"%ju",	"pkts",		TABLE_ALIGN_RIGHT },
	{ 10, "OWD last",	"%f",	"secs",		TABLE_ALIGN_RIGHT },
	{ 10, "OWD mean",	"%f",	"secs",		TABLE_ALIGN_RIGHT },
	{ 10, "Rate last",	"%f",	"pkt/sec",	TABLE_ALIGN_RIGHT },
	{ 10, "Rate mean",	"%f",	"pkt/sec",	TABLE_ALIGN_RIGHT },
	{ 10, "Drop",		"%ju",	"pkts",		TABLE_ALIGN_RIGHT },
	{ 10, "Skip",		"%ju",	"pkts",		TABLE_ALIGN_RIGHT },
	{ 10, "Time",		"%f",	"secs",		TABLE_ALIGN_RIGHT }
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

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, int verbose, struct node *n)
{
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			table_row(&stats_table,
				node_name_short(n),
				hist_total(&s->histograms[STATS_OWD]),
				hist_total(&s->histograms[STATS_TIME]),
				hist_last(&s->histograms[STATS_OWD]),
				hist_mean(&s->histograms[STATS_OWD]),
				1.0 / hist_last(&s->histograms[STATS_GAP_RECEIVED]),
				1.0 / hist_mean(&s->histograms[STATS_GAP_RECEIVED]),
				hist_total(&s->histograms[STATS_REORDERED]),
				hist_total(&s->histograms[STATS_SKIPPED]),
				hist_mean(&s->histograms[STATS_TIME])
			);
			break;

		case STATS_FORMAT_JSON: {
			json_t *json_stats = stats_json_periodic(s, n);
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

enum stats_id stats_lookup_id(const char *name)
{
	for (int i = 0; i < STATS_COUNT; i++) {
		struct stats_desc *desc = &stats_metrics[i];

		if (!strcmp(desc->name, name))
			return i;
	}

	return -1;
}
