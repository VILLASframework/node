/** Hook functions to collect statistics
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _STATS_H_
#define _STATS_H_

/* Forward declarations */
struct path;

/** Print a table header for statistics printed by stats_line() */
void stats_header();

/** Print a single line of stats including received, sent, invalid and dropped packet counters */
int stats_line(struct path *p);

int stats_show(struct path *p);

/** Update histograms */
int stats_collect(struct path *p);

/** Create histograms */
int stats_start(struct path *p);

/** Destroy histograms */
int stats_stop(struct path *p);

/** Reset all statistic counters to zero */
int stats_reset(struct path *p);

#endif