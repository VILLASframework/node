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
#include <villas/hist.hpp>
#include <villas/timing.h>
#include <villas/node.h>
#include <villas/utils.hpp>
#include <villas/log.h>
#include <villas/node.h>
#include <villas/table.hpp>

using namespace villas;
using namespace villas::utils;

struct stats_metric_description stats_metrics[] = {
	{ "skipped",		STATS_METRIC_SMPS_SKIPPED,	"samples",	"Skipped samples and the distance between them" 		},
	{ "reordered",		STATS_METRIC_SMPS_REORDERED, 	"samples",	"Reordered samples and the distance between them" 		},
	{ "gap_sent",		STATS_METRIC_GAP_SAMPLE,	"seconds",	"Inter-message timestamps (as sent by remote)" 			},
	{ "gap_received",	STATS_METRIC_GAP_RECEIVED,	"seconds",	"Inter-message arrival time (as received by this instance)" 	},
	{ "owd",		STATS_METRIC_OWD,		"seconds",	"One-way-delay (OWD) of received messages" 			},
	{ "age",		STATS_METRIC_AGE,		"seconds",	"Processing time of packets within the from receive to sent" 	},
	{ "rtp.loss_fraction",	STATS_METRIC_RTP_LOSS_FRACTION,	"percent",	"Fraction lost since last RTP SR/RR."				},
	{ "rtp.pkts_lost",	STATS_METRIC_RTP_PKTS_LOST,	"packets",	"Cumulative number of packtes lost" 				},
	{ "rtp.jitter",		STATS_METRIC_RTP_JITTER,	"seconds",	"Interarrival jitter" 						},
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
		new (&s->histograms[i]) Hist(buckets, warmup);

	s->state = STATE_INITIALIZED;

	return 0;
}

int stats_destroy(struct stats *s)
{
	assert(s->state != STATE_DESTROYED);

	for (int i = 0; i < STATS_METRIC_COUNT; i++)
		s->histograms[i].~Hist();

	s->state = STATE_DESTROYED;

	return 0;
}

void stats_update(struct stats *s, enum stats_metric id, double val)
{
	assert(s->state == STATE_INITIALIZED);

	s->histograms[id].put(val);
}

json_t * stats_json(struct stats *s)
{
	assert(s->state == STATE_INITIALIZED);

	json_t *obj = json_object();

	for (int i = 0; i < STATS_METRIC_COUNT; i++) {
		struct stats_metric_description *d = &stats_metrics[i];
		const Hist &h = s->histograms[i];

		json_object_set_new(obj, d->name, h.toJson());
	}

	return obj;
}

void stats_reset(struct stats *s)
{
	assert(s->state == STATE_INITIALIZED);

	for (int i = 0; i < STATS_METRIC_COUNT; i++)
		s->histograms[i].reset();
}

static std::vector<TableColumn> stats_columns = {
	{ 10, TableColumn::align::LEFT,		"Node",		"%s"			},
	{ 10, TableColumn::align::RIGHT,	"Recv",		"%ju",	"pkts"		},
	{ 10, TableColumn::align::RIGHT,	"Sent",		"%ju",	"pkts"		},
	{ 10, TableColumn::align::RIGHT,	"Drop",		"%ju",	"pkts"		},
	{ 10, TableColumn::align::RIGHT,	"Skip",		"%ju",	"pkts"		},
	{ 10, TableColumn::align::RIGHT,	"OWD last",	"%lf",	"secs"		},
	{ 10, TableColumn::align::RIGHT,	"OWD mean",	"%lf",	"secs"		},
	{ 10, TableColumn::align::RIGHT,	"Rate last",	"%lf",	"pkt/sec"	},
	{ 10, TableColumn::align::RIGHT,	"Rate mean",	"%lf",	"pkt/sec"	},
	{ 10, TableColumn::align::RIGHT,	"Age mean",	"%lf",	"secs"		},
	{ 10, TableColumn::align::RIGHT,	"Age Max",	"%lf",	"sec"		}
};

static Table stats_table = Table(stats_columns);

void stats_print_header(enum stats_format fmt)
{
	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			stats_table.header();
			break;

		default: { }
	}
}

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, struct node *n)
{
	assert(s->state == STATE_INITIALIZED);

	switch (fmt) {
		case STATS_FORMAT_HUMAN:
			stats_table.row(11,
				node_name_short(n),
				(uintmax_t)    s->histograms[STATS_METRIC_OWD].getTotal(),
				(uintmax_t)    s->histograms[STATS_METRIC_AGE].getTotal(),
				(uintmax_t)    s->histograms[STATS_METRIC_SMPS_REORDERED].getTotal(),
				(uintmax_t)    s->histograms[STATS_METRIC_SMPS_SKIPPED].getTotal(),
				(double)       s->histograms[STATS_METRIC_OWD].getLast(),
				(double)       s->histograms[STATS_METRIC_OWD].getMean(),
				(double) 1.0 / s->histograms[STATS_METRIC_GAP_RECEIVED].getLast(),
				(double) 1.0 / s->histograms[STATS_METRIC_GAP_RECEIVED].getMean(),
				(double)       s->histograms[STATS_METRIC_AGE].getMean(),
				(double)       s->histograms[STATS_METRIC_AGE].getHighest()
			);
			break;

		case STATS_FORMAT_JSON: {
			json_t *json_stats = json_pack("{ s: s, s: i, s: i, s: i, s: i, s: f, s: f, s: f, s: f, s: f, s: f }",
				"node", node_name(n),
				"recv",            s->histograms[STATS_METRIC_OWD].getTotal(),
				"sent",            s->histograms[STATS_METRIC_AGE].getTotal(),
				"dropped",         s->histograms[STATS_METRIC_SMPS_REORDERED].getTotal(),
				"skipped",         s->histograms[STATS_METRIC_SMPS_SKIPPED].getTotal(),
				"owd_last",  1.0 / s->histograms[STATS_METRIC_OWD].getLast(),
				"owd_mean",  1.0 / s->histograms[STATS_METRIC_OWD].getMean(),
				"rate_last", 1.0 / s->histograms[STATS_METRIC_GAP_SAMPLE].getLast(),
				"rate_mean", 1.0 / s->histograms[STATS_METRIC_GAP_SAMPLE].getMean(),
				"age_mean",        s->histograms[STATS_METRIC_AGE].getMean(),
				"age_max",         s->histograms[STATS_METRIC_AGE].getHighest()
			);
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
				s->histograms[i].print(verbose);
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

	const Hist &h = s->histograms[sm];
	union signal_data d;

	switch (st) {
		case STATS_TYPE_TOTAL:
			d.i = h.getTotal();
			break;

		case STATS_TYPE_LAST:
			d.f = h.getLast();
			break;

		case STATS_TYPE_HIGHEST:
			d.f = h.getHighest();
			break;

		case STATS_TYPE_LOWEST:
			d.f = h.getLowest();
			break;

		case STATS_TYPE_MEAN:
			d.f = h.getMean();
			break;

		case STATS_TYPE_STDDEV:
			d.f = h.getStddev();
			break;

		case STATS_TYPE_VAR:
			d.f = h.getVar();
			break;

		default:
			d.f = -1;
	}

	return d;
}
