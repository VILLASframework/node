/** Sending statistics to another node.
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

static struct plugin p = {
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

REGISTER_PLUGIN(&p)
	
/** @} */