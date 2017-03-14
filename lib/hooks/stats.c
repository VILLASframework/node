/** Statistic hooks.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"
#include "stats.h"
#include "path.h"

static int hook_stats(struct hook *h, int when, struct hook_info *j)
{
	struct stats *s = hook_storage(h, when, sizeof(struct stats), (ctor_cb_t) stats_init, (dtor_cb_t) stats_destroy);
	
	switch (when) {
		case HOOK_INIT:
			if (j->path)
				j->path->stats = s;
			break;

		case HOOK_READ:
			assert(j->smps);
		
			stats_collect(s->delta, j->smps, j->cnt);
			stats_commit(s, s->delta);
			break;

		case HOOK_PATH_STOP:
			stats_print(s, 1);
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

/** @todo This is untested */
static int hook_stats_send(struct hook *h, int when, struct hook_info *j)
{
	struct private {
		struct node *dest;
		struct stats *stats;
		int ratio;
	} *private = hook_storage(h, when, sizeof(*private), NULL, NULL);
	
	switch (when) {
		case HOOK_INIT:
			assert(j->nodes);
			assert(j->path);
		
			if (!h->parameter)
				error("Missing parameter for hook '%s'", plugin_name(h));
			
			private->dest = list_lookup(j->nodes, h->parameter);
			if (!private->dest)
				error("Invalid destination node '%s' for hook '%s'", h->parameter, plugin_name(h));
			break;
			
		case HOOK_PATH_START:
			node_start(private->dest);
			break;

		case HOOK_PATH_STOP:
			node_stop(private->dest);
			break;

		case HOOK_READ:
			stats_send(private->stats, private->dest);
			break;
	}
	
	return 0;
}

static struct plugin p1 = {
	.name		= "stats",
	.description	= "Collect statistics for the current path",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 2,
		.cb	= hook_stats,
		.type	= HOOK_STORAGE | HOOK_PATH | HOOK_READ | HOOK_PERIODIC
	}
};

static struct plugin p2 = {
	.name		= "stats_send",
	.description	= "Send path statistics to another node",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_stats_send,
		.type	= HOOK_STORAGE | HOOK_PATH | HOOK_READ
	}
};

REGISTER_PLUGIN(&p1)
REGISTER_PLUGIN(&p2)

/** @} */