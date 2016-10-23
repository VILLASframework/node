/** Statistic-related hook functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include "hooks.h"
#include "sample.h"
#include "path.h"
#include "utils.h"
#include "stats.h"

extern struct list *hook_nodes;

REGISTER_HOOK("stats", "Collect statistics for the current path", 2, 1, hook_stats, HOOK_STATS)
int hook_stats(struct hook *h, int when, struct hook_info *j)
{
	struct stats *s = hook_storage(h, when, sizeof(struct stats));
	
	switch (when) {
		case HOOK_INIT:
			stats_init(s);
			break;

		case HOOK_READ:
			assert(j->smps);
		
			stats_collect(s, j->smps, j->cnt);
			break;

		case HOOK_PATH_STOP:
			stats_print(s);
			break;
		
		case HOOK_DESTROY:
			stats_destroy(s);
			break;

		case HOOK_PATH_RESTART:
			stats_reset(s);
			break;
			
		case HOOK_PERIODIC:
			assert(j->path);

			stats_print_periodic(s, j->path);
			break;
	}
	
	return j->cnt;
}

#if 0 /* currently broken */
REGISTER_HOOK("stats_send", "Send path statistics to another node", 99, 0, hook_stats_send, HOOK_STORAGE | HOOK_DESTROY | HOOK_PERIODIC | HOOK_PATH)
int hook_stats_send(struct hook *h, int when, struct hook_info *i)
{
	struct private {
		struct node *dest;
		int ratio;
	} *private = hook_storage(h, when, sizeof(*private));
	
	switch (when) {
		case HOOK_DESTROY:
			if (!h->parameter)
				error("Missing parameter for hook '%s'", h->name);
			
			if (!hook_nodes)
				error("Missing reference to node list for hook '%s", h->name);

			private->dest = list_lookup(hook_nodes, h->parameter);
			if (!private->dest)
				error("Invalid destination node '%s' for hook '%s'", h->parameter, h->name);

			node_start(private->dest);
			
			break;

		case HOOK_PERIODIC: {
			stats_send(s, node);
			break;
		}
	}
	
	return 0;
}
#endif