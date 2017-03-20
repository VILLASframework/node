/** Statistic collection.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

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
	STATS_SKIPPED,		/**< Counter for skipped messages due to hooks */
	STATS_DROPPED,		/**< Counter for dropped messages due to reordering */
	STATS_GAP_SEQUENCE,	/**< Histogram of sequence number displacement of received messages */
	STATS_GAP_SAMPLE,	/**< Histogram for inter message timestamps (as sent by remote) */
	STATS_GAP_RECEIVED,	/**< Histogram for inter message arrival time (as seen by this instance) */
	STATS_OWD,		/**< Histogram for one-way-delay (OWD) of received messages */
	STATS_COUNT		/**< Just here to have an updated number of statistics */
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

int stats_init(struct stats *s);

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

#endif /* _STATS_H_ */