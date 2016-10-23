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

struct stats {
	struct {
		uintmax_t invalid;	/**< Counter for invalid messages */
		uintmax_t skipped;	/**< Counter for skipped messages due to hooks */
		uintmax_t dropped;	/**< Counter for dropped messages due to reordering */
	} counter;

	struct {
		struct hist owd;	/**< Histogram for one-way-delay (OWD) of received messages */
		struct hist gap_msg;	/**< Histogram for inter message timestamps (as sent by remote) */
		struct hist gap_recv;	/**< Histogram for inter message arrival time (as seen by this instance) */
		struct hist gap_seq;	/**< Histogram of sequence number displacement of received messages */
	} histogram;
	
	struct sample *last;
};

int stats_init(struct stats *s);

void stats_destroy(struct stats *s);

void stats_collect(struct stats *s, struct sample *smps[], size_t cnt);

#ifdef WITH_JANSSON
json_t * stats_json(struct stats *s);
#endif

void stats_reset(struct stats *s);

void stats_print_header();

void stats_print_periodic(struct stats *s, struct path *p);

void stats_print(struct stats *s);

void stats_send(struct stats *s, struct node *n);

#endif /* _STATS_H_ */