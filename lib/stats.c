/** Statistic collection.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/utils.h>
#include <villas/log.h>
#include <villas/node.h>
#include <villas/table.h>

struct stats_metric_description stats_metrics[] = {
	{ "skipped",		STATS_METRIC_SKIPPED,		"samples", "Skipped samples and the distance between them",		25 },
	{ "reordered",		STATS_METRIC_REORDERED, 	"samples", "Reordered samples and the distance between them",		25 },
	{ "gap_sent",		STATS_METRIC_GAP_SAMPLE,	"seconds", "Inter-message timestamps (as sent by remote)",		25 },
	{ "gap_received",	STATS_METRIC_GAP_RECEIVED,	"seconds", "Inter-message arrival time (as received by this instance)",	25 },
	{ "owd",		STATS_METRIC_OWD,		"seconds", "One-way-delay (OWD) of received messages",			25 }
};

struct stats_type_description stats_types[] = {
	{ "last",		STATS_TYPE_LAST,	SIGNAL_TYPE_FLOAT },
	{ "highest",		STATS_TYPE_HIGHEST,	SIGNAL_TYPE_FLOAT },
	{ "lowest",		STATS_TYPE_LOWEST,	SIGNAL_TYPE_FLOAT },
	{ "mean",		STATS_TYPE_MEAN,	SIGNAL_TYPE_FLOAT },
	{ "var",		STATS_TYPE_VAR,		SIGNAL_TYPE_FLOAT },
	{ "stddev",		STATS_TYPE_STDDEV,	SIGNAL_TYPE_FLOAT },
	{ "total",		STATS_TYPE_TOTAL,	SIGNAL_TYPE_INTEGER }
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

enum stats_metric stats_lookup_metric(const char *str)
{
	for (int i = 0; i < STATS_METRIC_COUNT; i++) {
		struct stats_metric_description *d = &stats_metrics[i];

		if (!strcmp(str, d->name))
			return d->metric;
	}

	return STATS_METRIC_INVALID;
}

enum stats_type stats_lookup_type(const char *str)
{
	for (int i = 0; i < STATS_TYPE_COUNT; i++) {
		struct stats_type_description *d = &stats_types[i];

		if (!strcmp(str, d->name))
			return d->type;
	}

	return STATS_TYPE_INVALID;
}

int stats_init(struct stats *s, int buckets, int warmup)
{
	assert(s->state == STATE_DESTROYED);

	for (int i = 0; i < STATS_METRIC_COUNT; i++)
		hist_init(&s->histograms[i], buckets, warmup);

	s->state = STATE_INITIALIZED;

	return 0;
}

int stats_destroy(struct stats *s)
{
	assert(s->state != STATE_DESTROYED);

	for (int i = 0; i < STATS_METRIC_COUNT; i++)
		hist_destroy(&s->histograms[i]);

	s->state = STATE_DESTROYED;

	return 0;
}

void stats_update(struct stats *s, enum stats_metric id, double val)
{
	assert(s->state == STATE_INITIALIZED);

	hist_put(&s->histograms[id], val);
}

json_t * stats_json(struct stats *s)
{
	assert(s->state == STATE_INITIALIZED);

	json_t *obj = json_object();

	for (int i = 0; i < STATS_METRIC_COUNT; i++) {
		struct stats_metric_description *d = &stats_metrics[i];
		struct hist *h = &s->histograms[i];

		json_object_set_new(obj, d->name, hist_json(h));
	}

	return obj;
}

json_t * stats_json_periodic(struct stats *s, struct node *n)
{
	assert(s->state == STATE_INITIALIZED);

	return json_pack("{ s: s, s: i, s: f, s: f, s: i, s: i }",
		"node", node_name(n),
		"processed", hist_total(&s->histograms[STATS_METRIC_OWD]),
		"owd", hist_last(&s->histograms[STATS_METRIC_OWD]),
		"rate", 1.0 / hist_last(&s->histograms[STATS_METRIC_GAP_SAMPLE]),
		"dropped", hist_total(&s->histograms[STATS_METRIC_REORDERED]),
		"skipped", hist_total(&s->histograms[STATS_METRIC_SKIPPED])
	);
}

void stats_reset(struct stats *s)
{
	assert(s->state == STATE_INITIALIZED);

	for (int i = 0; i < STATS_METRIC_COUNT; i++)
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
	{ 10, "Skip",		"%ju",	"pkts",		TABLE_ALIGN_RIGHT }
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

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, int verbose, struct node *n)
{
	assert(s->state == STATE_INITIALIZED);

	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			table_row(&stats_table,
				node_name_short(n),
				hist_total(&s->histograms[STATS_METRIC_OWD]),
				hist_last(&s->histograms[STATS_METRIC_OWD]),
				hist_mean(&s->histograms[STATS_METRIC_OWD]),
				1.0 / hist_last(&s->histograms[STATS_METRIC_GAP_RECEIVED]),
				1.0 / hist_mean(&s->histograms[STATS_METRIC_GAP_RECEIVED]),
				hist_total(&s->histograms[STATS_METRIC_REORDERED]),
				hist_total(&s->histograms[STATS_METRIC_SKIPPED])
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
	assert(s->state == STATE_INITIALIZED);

	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			for (int i = 0; i < STATS_METRIC_COUNT; i++) {
				struct stats_metric_description *d = &stats_metrics[i];

				info("%s: %s", d->name, d->desc);
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

union signal_data stats_get_value(const struct stats *s, enum stats_metric sm, enum stats_type st)
{
	assert(s->state == STATE_INITIALIZED);

	const struct hist *h = &s->histograms[sm];
	union signal_data d;

	switch (st) {
		case STATS_TYPE_TOTAL:
			d.i = h->total;
			break;

		case STATS_TYPE_LAST:
			d.f = h->last;
			break;

		case STATS_TYPE_HIGHEST:
			d.f = h->highest;
			break;

		case STATS_TYPE_LOWEST:
			d.f = h->lowest;
			break;

		case STATS_TYPE_MEAN:
			d.f = hist_mean(h);
			break;

		case STATS_TYPE_STDDEV:
			d.f = hist_stddev(h);
			break;

		case STATS_TYPE_VAR:
			d.f = hist_var(h);
			break;

		default:
			d.f = -1;
	}

	return d;
}
