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

#ifndef _STATS_H_
#define _STATS_H_

#include <stdint.h>

#ifdef WITH_JANSSON
  #include <jansson.h>
#endif

#include "hist.h"

/* Forward declarations */
struct sample;
struct path;
struct node;

enum stats_format {
	STATS_FORMAT_HUMAN,
	STATS_FORMAT_JSON,
	STATS_FORMAT_MATLAB
};

enum stats_id {
	STATS_SKIPPED,		/**< Counter for skipped samples due to hooks. */
	STATS_REORDERED,	/**< Counter for reordered samples. */
	STATS_GAP_SEQUENCE,	/**< Histogram of sequence number displacement of received samples. */
	STATS_GAP_SAMPLE,	/**< Histogram for inter sample timestamps (as sent by remote). */
	STATS_GAP_RECEIVED,	/**< Histogram for inter sample arrival time (as seen by this instance). */
	STATS_OWD,		/**< Histogram for one-way-delay (OWD) of received samples. */
	STATS_COUNT		/**< Just here to have an updated number of statistics. */
};

struct stats_delta {
	double values[STATS_COUNT];

	int update;		/**< Bitmask of stats_id. Only those which are masked will be updated */
	struct sample *last;
};

struct stats {
	struct hist histograms[STATS_COUNT];

	struct stats_delta *delta;
};

int stats_init(struct stats *s, int buckets, int warmup);

void stats_destroy(struct stats *s);

void stats_update(struct stats_delta *s, enum stats_id id, double val);

void stats_collect(struct stats_delta *s, struct sample *smps[], size_t cnt);

int stats_commit(struct stats *s, struct stats_delta *d);

#ifdef WITH_JANSSON
json_t * stats_json(struct stats *s);
#endif

void stats_reset(struct stats *s);

void stats_print_header();

void stats_print_periodic(struct stats *s, FILE *f, enum stats_format fmt, int verbose, struct path *p);

void stats_print(struct stats *s, FILE *f, enum stats_format fmt, int verbose);

void stats_send(struct stats *s, struct node *n);

enum stats_id stats_lookup_id(const char *name);

#endif /* _STATS_H_ */