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

int stats_line(struct path *p);

int stats_show(struct path *p);

int stats_collect(struct path *p);

int stats_start(struct path *p);

int stats_stop(struct path *p);

int stats_reset(struct path *p);

#endif