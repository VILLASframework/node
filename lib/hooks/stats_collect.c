/** Statistic hooks.
 *
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
	int warmup;
	int buckets;

	FILE *output;
	const char *uri;
};

static int stats_collect_init(struct hook *h)
{
	struct stats_collect *p = h->_vd;

	/* Register statistic object to path.
	 *
	 * This allows the path code to update statistics. */
	if (h->path)
		h->path->stats = &p->stats;

	/* Set default values */
	p->format = STATS_FORMAT_HUMAN;
	p->verbose = 0;
	p->warmup = 500;
	p->buckets = 20;
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

	stats_init(&p->stats, p->buckets, p->warmup);

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
	config_setting_lookup_int(cfg, "warmup", &p->warmup);
	config_setting_lookup_int(cfg, "buckets", &p->buckets);
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

static struct plugin p = {
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

REGISTER_PLUGIN(&p)

/** @} */
