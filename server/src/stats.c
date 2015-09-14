/** Hook functions to collect statistics
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include "stats.h"
#include "path.h"
#include "timing.h"
#include "utils.h"

void stats_header()
{
	info("%-32s :   %-8s %-8s %-8s %-8s %-8s",
		"Source " MAG("=>") " Destination", "#Sent", "#Recv", "#Drop", "#Skip", "#Invalid");
	line();
}

int stats_line(struct path *p)
{
	char buf[33];
	path_print(p, buf, sizeof(buf));

	info("%-32s :   %-8u %-8u %-8u %-8u %-8u", buf,
		p->sent, p->received, p->dropped, p->skipped, p->invalid);

	return 0;
}

int stats_show(struct path *p)
{
	if (p->hist_delay.total)    { info("One-way delay:");        hist_print(&p->hist_delay); }
	if (p->hist_gap.total)      { info("Message gap time:");     hist_print(&p->hist_gap);   }
	if (p->hist_sequence.total) { info("Sequence number gaps:"); hist_print(&p->hist_sequence);   }

	return 0;
}

int stats_collect(struct path *p)
{
	int dist = p->current->sequence - (int32_t) p->previous->sequence;

	struct timespec ts1 = MSG_TS(p->current);
	struct timespec ts2 = MSG_TS(p->previous);

	hist_put(&p->hist_sequence, dist);
	hist_put(&p->hist_delay, time_delta(&ts1, &p->ts_recv));
	hist_put(&p->hist_gap, time_delta(&ts2, &ts1));

	return 0;
}

int stats_start(struct path *p)
{
	/** @todo Allow configurable bounds for histograms */
	hist_create(&p->hist_sequence, -HIST_SEQ, +HIST_SEQ, 1);
	hist_create(&p->hist_delay, 0, 2, 100e-3);
	hist_create(&p->hist_gap, 0, 40e-3, 1e-3);

	return 0;
}

int stats_stop(struct path *p)
{
	hist_destroy(&p->hist_sequence);
	hist_destroy(&p->hist_delay);
	hist_destroy(&p->hist_gap);

	return 0;
}

int stats_reset(struct path *p)
{
	hist_reset(&p->hist_sequence);
	hist_reset(&p->hist_delay);
	hist_reset(&p->hist_gap);

	return 0;
}
