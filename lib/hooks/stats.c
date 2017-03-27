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

struct stats_collect {
	struct stats stats;

	enum stats_format format;
	int verbose;
	
	FILE *output;
	const char *uri;
};

static int stats_collect_init(struct hook *h)
{
	struct stats_collect *p = h->_vd;

	stats_init(&p->stats);

	/* Register statistic object to path.
	 *
	 * This allows the path code to update statistics. */
	if (h->path)
		h->path->stats = &p->stats;
	
	/* Set default values */
	p->format = STATS_FORMAT_HUMAN;
	p->verbose = 0;
	p->uri = NULL;
	p->output = stdout;
	
	return 0;
}

static int stats_collect_destroy(struct hook *h)
{
	struct stats_collect *p = h->_vd;
	
	stats_destroy(&p->stats);
	
	return 0;
}

static int stats_collect_start(struct hook *h)
{
	struct stats_collect *p = h->_vd;
	
	if (p->uri) {
		p->output = fopen(p->uri, "w+");
		if (!p->output)
			error("Failed to open file %s for writing", p->uri);
	}
	
	return 0;
}

static int stats_collect_stop(struct hook *h)
{
	struct stats_collect *p = h->_vd;
	
	stats_print(&p->stats, p->output, p->format, p->verbose);

	if (p->uri)
		fclose(p->output);
	
	return 0;
}

static int stats_collect_restart(struct hook *h)
{
	struct stats_collect *p = h->_vd;

	stats_reset(&p->stats);
	
	return 0;
}

static int stats_collect_periodic(struct hook *h)
{
	struct stats_collect *p = h->_vd;

	stats_print_periodic(&p->stats, p->output, p->format, p->verbose, h->path);
	
	return 0;
}

static int stats_collect_parse(struct hook *h, config_setting_t *cfg)
{
	struct stats_collect *p = h->_vd;

	const char *format;
	if (config_setting_lookup_string(cfg, "format", &format)) {
		if      (!strcmp(format, "human"))
			p->format = STATS_FORMAT_HUMAN;
		else if (!strcmp(format, "json"))
			p->format = STATS_FORMAT_JSON;
		else if (!strcmp(format, "matlab"))
			p->format = STATS_FORMAT_MATLAB;
		else
			cerror(cfg, "Invalid statistic output format: %s", format);
	}
	
	config_setting_lookup_int(cfg, "verbose", &p->verbose);
	config_setting_lookup_string(cfg, "output", &p->uri);
	
	return 0;
}

static int stats_collect_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct stats_collect *p = h->_vd;
	
	stats_collect(p->stats.delta, smps, *cnt);
	stats_commit(&p->stats, p->stats.delta);
	
	return 0;
}

struct stats_send {
	struct node *dest;

	enum {
		STATS_SEND_MODE_PERIODIC,
		STATS_SEND_MODE_READ
	} mode;

	int decimation;
};

static int stats_send_init(struct hook *h)
{
	struct stats_send *p = h->_vd;

	p->decimation = 1;
	p->mode = STATS_SEND_MODE_PERIODIC;
	
	return 0;
}

static int stats_send_parse(struct hook *h, config_setting_t *cfg)
{
	struct stats_send *p = h->_vd;

	assert(h->path && h->path->super_node);

	const char *dest, *mode;

	if (config_setting_lookup_string(cfg, "destination", &dest)) {
		p->dest = list_lookup(&h->path->super_node->nodes, dest);
		if (!p->dest)
			cerror(cfg, "Invalid destination node '%s' for hook '%s'", dest, plugin_name(h->_vt));
	}
	else
		cerror(cfg, "Missing setting 'destination' for hook '%s'", plugin_name(h->_vt));

	if (config_setting_lookup_string(cfg, "destination", &mode)) {
		if      (!strcmp(mode, "periodic"))
			p->mode = STATS_SEND_MODE_PERIODIC;
		else if (!strcmp(mode, "read"))
			p->mode = STATS_SEND_MODE_READ;
		else
			cerror(cfg, "Invalid value '%s' for setting 'mode' of hook '%s'", mode, plugin_name(h->_vt));
	}

	config_setting_lookup_int(cfg, "decimation", &p->decimation);

	return 0;
}

static int stats_send_start(struct hook *h)
{
	struct stats_send *p = h->_vd;

	if (p->dest->state != STATE_STOPPED)
		node_start(p->dest);
	
	return 0;
}

static int stats_send_stop(struct hook *h)
{
	struct stats_send *p = h->_vd;

	if (p->dest->state != STATE_STOPPED)
		node_stop(p->dest);
	
	return 0;
}

static int stats_send_periodic(struct hook *h)
{
	struct stats_send *p = h->_vd;
	
	if (p->mode == STATS_SEND_MODE_PERIODIC)
		stats_send(h->path->stats, p->dest);

	return 0;
}

static int stats_send_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct stats_send *p = h->_vd;

	assert(h->path->stats);

	if (p->mode == STATS_SEND_MODE_READ) {
		size_t processed = h->path->stats->histograms[STATS_OWD].total;
		if (processed % p->decimation == 0)
			stats_send(h->path->stats, p->dest);
	}
	
	return 0;
}

static struct plugin p1 = {
	.name		= "stats",
	.description	= "Collect statistics for the current path",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 2,
		.init	= stats_collect_init,
		.destroy= stats_collect_destroy,
		.start	= stats_collect_start,
		.stop	= stats_collect_stop,
		.read	= stats_collect_read,
		.restart= stats_collect_restart,
		.periodic= stats_collect_periodic,
		.parse	= stats_collect_parse,
		.size	= sizeof(struct stats_collect),
	}
};

static struct plugin p2 = {
	.name		= "stats_send",
	.description	= "Send path statistics to another node",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.init	= stats_send_init,
		.parse	= stats_send_parse,
		.start	= stats_send_start,
		.stop	= stats_send_stop,
		.periodic= stats_send_periodic,
		.read	= stats_send_read,
		.size	= sizeof(struct stats_send)
	}
};

REGISTER_PLUGIN(&p1)
REGISTER_PLUGIN(&p2)

/** @} */