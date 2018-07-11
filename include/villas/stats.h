/** Statistic collection.
 *
 * @file
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

#pragma once

#include <stdint.h>
#include <jansson.h>

#include <villas/hist.h>

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

enum stats_id {
	STATS_SKIPPED,		/**< Counter for skipped samples due to hooks. */
	STATS_TIME,		/**< The processing time per sample within VILLAsnode. */
	STATS_REORDERED,	/**< Counter for reordered samples. */
	STATS_GAP_SAMPLE,	/**< Histogram for inter sample timestamps (as sent by remote). */
	STATS_GAP_RECEIVED,	/**< Histogram for inter sample arrival time (as seen by this instance). */
	STATS_OWD,		/**< Histogram for one-way-delay (OWD) of received samples. */
	STATS_COUNT		/**< Just here to have an updated number of statistics. */
};

struct stats_delta {
	double values[STATS_COUNT];

	int update;		/**< Bitmask of stats_id. Only those which are masked will be updated */
};

struct stats {
	struct hist histograms[STATS_COUNT];

	struct stats_delta *delta;
};

int stats_lookup_format(const char *str);

int stats_init(struct stats *s, int buckets, int warmup);

int stats_destroy(struct stats *s);

void stats_update(struct stats *s, enum stats_id id, double val);

void stats_collect(struct stats *s, struct sample *smps[], size_t *cnt);

int stats_commit(struct stats *s);

json_t * stats_json(struct stats *s);

void stats_reset(struct stats *s);

void stats_print_header(enum stats_format fmt);
void stats_print_footer(enum stats_format fmt);

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, int verbose, struct node *p);

void stats_print(struct stats *s, FILE *f, enum stats_format fmt, int verbose);

enum stats_id stats_lookup_id(const char *name);

#ifdef __cplusplus
}
#endif
