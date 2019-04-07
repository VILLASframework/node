/** Statistic collection.
 *
 * @file
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

#pragma once

#include <stdint.h>
#include <jansson.h>

#include <villas/common.h>
#include <villas/hist.h>
#include <villas/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct sample;
struct node;

enum stats_format {
	STATS_FORMAT_HUMAN,
	STATS_FORMAT_JSON,
	STATS_FORMAT_MATLAB
};

enum stats_metric {
	STATS_METRIC_INVALID = -1,

	STATS_METRIC_SMPS_SKIPPED,	/**< Counter for skipped samples due to hooks. */
	STATS_METRIC_SMPS_REORDERED,	/**< Counter for reordered samples. */

	/* Timings */
	STATS_METRIC_GAP_SAMPLE,	/**< Histogram for inter sample timestamps (as sent by remote). */
	STATS_METRIC_GAP_RECEIVED,	/**< Histogram for inter sample arrival time (as seen by this instance). */
	STATS_METRIC_OWD,		/**< Histogram for one-way-delay (OWD) of received samples. */
	STATS_METRIC_AGE,		/**< Processing time of packets within VILLASnode. */

	/* RTP metrics */
	STATS_METRIC_RTP_LOSS_FRACTION,	/**< Fraction lost since last RTP SR/RR. */
	STATS_METRIC_RTP_PKTS_LOST,	/**< Cumul. no. pkts lost. */
	STATS_METRIC_RTP_JITTER,	/**< Interarrival jitter. */

	/* Always last */
	STATS_METRIC_COUNT		/**< Just here to have an updated number of statistics. */
};

enum stats_type {
	STATS_TYPE_INVALID = -1,
	STATS_TYPE_LAST,
	STATS_TYPE_HIGHEST,
	STATS_TYPE_LOWEST,
	STATS_TYPE_MEAN,
	STATS_TYPE_VAR,
	STATS_TYPE_STDDEV,
	STATS_TYPE_TOTAL,
	STATS_TYPE_COUNT
};

struct stats_metric_description {
	const char *name;
	enum stats_metric metric;
	const char *unit;
	const char *desc;
};

struct stats_type_description {
	const char *name;
	enum stats_type type;
	enum signal_type signal_type;
};

struct stats {
	enum state state;
	struct hist histograms[STATS_METRIC_COUNT];
};

extern struct stats_metric_description stats_metrics[];
extern struct stats_type_description stats_types[];

int stats_lookup_format(const char *str);

enum stats_metric stats_lookup_metric(const char *str);

enum stats_type stats_lookup_type(const char *str);

int stats_init(struct stats *s, int buckets, int warmup);

int stats_destroy(struct stats *s);

void stats_update(struct stats *s, enum stats_metric id, double val);

json_t * stats_json(struct stats *s);

void stats_reset(struct stats *s);

void stats_print_header(enum stats_format fmt);

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, struct node *p);

void stats_print(struct stats *s, FILE *f, enum stats_format fmt, int verbose);

union signal_data stats_get_value(const struct stats *s, enum stats_metric sm, enum stats_type st);

#ifdef __cplusplus
}
#endif
