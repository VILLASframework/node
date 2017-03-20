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

struct stats_hook {
	struct stats stats;

	enum stats_format format;
	int verbose;
	
	FILE *output;
	const char *uri;
};

static int hook_stats(struct hook *h, int when, struct hook_info *j)
{
	struct stats_hook *p = (struct stats_hook *) h->_vd;
	
	switch (when) {
		case HOOK_INIT:
			stats_init(&p->stats);
		
			/* Register statistic object to path.
			 *
			 * This allows the path code to update statistics. */
			if (j->path)
				j->path->stats = &p->stats;
			
			/* Set default values */
			p->format = STATS_FORMAT_HUMAN;
			p->verbose = 0;
			p->uri = NULL;
			p->output = stdout;
			
			break;
		
		case HOOK_PARSE: {
			const char *format;
			if (config_setting_lookup_string(h->cfg, "format", &format)) {
				if      (!strcmp(format, "human"))
					p->format = STATS_FORMAT_HUMAN;
				else if (!strcmp(format, "json"))
					p->format = STATS_FORMAT_JSON;
				else if (!strcmp(format, "matlab"))
					p->format = STATS_FORMAT_MATLAB;
				else
					cerror(h->cfg, "Invalid statistic output format: %s", format);
			}
			
			config_setting_lookup_int(h->cfg, "verbose", &p->verbose);
			config_setting_lookup_string(h->cfg, "output", &p->uri);

			break;
		}

		case HOOK_DESTROY:
			stats_destroy(&p->stats);
			break;

		case HOOK_READ:
			assert(j->samples);
		
			stats_collect(p->stats.delta, j->samples, j->count);
			stats_commit(&p->stats, p->stats.delta);
			break;
			
		case HOOK_PATH_START:
			if (p->uri) {
				p->output = fopen(p->uri, "w+");
				if (!p->output)
					error("Failed to open file %s for writing", p->uri);
			}
			break;

		case HOOK_PATH_STOP:
			stats_print(&p->stats, p->output, p->format, p->verbose);

			if (p->uri)
				fclose(p->output);

			break;

		case HOOK_PATH_RESTART:
			stats_reset(&p->stats);
			break;
			
		case HOOK_PERIODIC:
			assert(j->path);

			stats_print_periodic(&p->stats, p->output, p->format, p->verbose, j->path);
			break;
	}
	
	return 0;
}

struct stats_send {
	struct node *dest;
	struct stats *stats;
	int ratio;
};

/** @todo This is untested */
static int hook_stats_send(struct hook *h, int when, struct hook_info *j)
{
	struct stats_send *p = (struct stats_send *) h->_vd;
	
	switch (when) {
		case HOOK_INIT:
			assert(j->nodes);
			assert(j->path);
		
			if (!h->cfg)
				error("Missing configuration for hook '%s'", plugin_name(h->_vt));
			
			const char *dest;
			
			if (!config_setting_lookup_string(h->cfg, "destination", &dest))
				cerror(h->cfg, "Missing setting 'destination' for hook '%s'", plugin_name(h->_vt));
			
			p->dest = list_lookup(j->nodes, dest);
			if (!p->dest)
				cerror(h->cfg, "Invalid destination node '%s' for hook '%s'", dest, plugin_name(h->_vt));
			break;
			
		case HOOK_PATH_START:
			node_start(p->dest);
			break;

		case HOOK_PATH_STOP:
			node_stop(p->dest);
			break;

		case HOOK_READ:
			stats_send(p->stats, p->dest);
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
		.size	= sizeof(struct stats_hook),
		.cb	= hook_stats,
		.when	= HOOK_STORAGE | HOOK_PARSE | HOOK_PATH | HOOK_READ | HOOK_PERIODIC
	}
};

static struct plugin p2 = {
	.name		= "stats_send",
	.description	= "Send path statistics to another node",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_stats_send,
		.when	= HOOK_STORAGE | HOOK_PATH | HOOK_READ
	}
};

REGISTER_PLUGIN(&p1)
REGISTER_PLUGIN(&p2)

/** @} */