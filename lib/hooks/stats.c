/** Statistic hooks.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <string.h>

#include <villas/common.h>
#include <villas/advio.h>
#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/stats.h>
#include <villas/node.h>
#include <villas/timing.h>

struct stats_collect {
	struct stats stats;

	enum stats_format format;
	int verbose;
	int warmup;
	int buckets;

	AFILE *output;
	char *uri;

	struct sample *last;
};

static int stats_collect_init(struct hook *h)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;

	/* Register statistic object to path.
	 *
	 * This allows the path code to update statistics. */
	if (h->node)
		h->node->stats = &p->stats;

	/* Set default values */
	p->format = STATS_FORMAT_HUMAN;
	p->verbose = 0;
	p->warmup = 500;
	p->buckets = 20;
	p->uri = NULL;

	return 0;
}

static int stats_collect_destroy(struct hook *h)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;

	if (p->uri)
		free(p->uri);

	return stats_destroy(&p->stats);
}

static int stats_collect_start(struct hook *h)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;

	if (p->uri) {
		p->output = afopen(p->uri, "w+");
		if (!p->output)
			error("Failed to open file %s for writing", p->uri);
	}

	return stats_init(&p->stats, p->buckets, p->warmup);
}

static int stats_collect_stop(struct hook *h)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;

	stats_print(&p->stats, p->uri ? p->output->file : stdout, p->format, p->verbose);

	if (p->uri)
		afclose(p->output);

	return 0;
}

static int stats_collect_restart(struct hook *h)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;

	stats_reset(&p->stats);

	return 0;
}

static int stats_collect_periodic(struct hook *h)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;

	stats_print_periodic(&p->stats, p->uri ? p->output->file : stdout, p->format, p->verbose, h->node);

	return 0;
}

static int stats_collect_parse(struct hook *h, json_t *cfg)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;

	int ret, fmt;
	json_error_t err;

	const char *format = NULL;
	const char *uri = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: b, s?: i, s?: i, s?: s }",
		"format", &format,
		"verbose", &p->verbose,
		"warmup", &p->warmup,
		"buckets", &p->buckets,
		"output", &uri
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(h->_vt));

	if (format) {
		fmt = stats_lookup_format(format);
		if (fmt < 0)
			jerror(&err, "Invalid statistic output format: %s", format);

		p->format = fmt;
	}

	if (uri)
		p->uri = strdup(uri);

	return 0;
}

static int stats_collect_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct stats_collect *p = (struct stats_collect *) h->_vd;
	struct stats *s = &p->stats;

	int dist;
	struct sample *previous = p->last;

	for (int i = 0; i < *cnt; i++) {
		struct sample *current = smps[i];

		if (previous) {
			if (current->flags & previous->flags & SAMPLE_HAS_TS_RECEIVED)
				stats_update(s, STATS_GAP_RECEIVED, time_delta(&previous->ts.received, &current->ts.received));

			if (current->flags & previous->flags & SAMPLE_HAS_TS_ORIGIN)
				stats_update(s, STATS_GAP_SAMPLE, time_delta(&previous->ts.origin, &current->ts.origin));

			if ((current->flags & SAMPLE_HAS_TS_ORIGIN) && (current->flags & SAMPLE_HAS_TS_RECEIVED))
				stats_update(s, STATS_OWD, time_delta(&current->ts.origin, &current->ts.received));

			if (current->flags & previous->flags & SAMPLE_HAS_SEQUENCE) {
				dist = current->sequence - (int32_t) previous->sequence;
				if (dist != 1)
					stats_update(s, STATS_REORDERED, dist);
			}
		}

		previous = current;
	}

	if (p->last)
		sample_decref(p->last);

	if (previous)
		sample_incref(previous);

	p->last = previous;

	stats_commit(&p->stats);

	return 0;
}

static struct plugin p = {
	.name		= "stats",
	.description	= "Collect statistics for the current path",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ,
		.priority	= 2,
		.init		= stats_collect_init,
		.destroy	= stats_collect_destroy,
		.start		= stats_collect_start,
		.stop		= stats_collect_stop,
		.process	= stats_collect_process,
		.restart	= stats_collect_restart,
		.periodic	= stats_collect_periodic,
		.parse		= stats_collect_parse,
		.size		= sizeof(struct stats_collect),
	}
};

REGISTER_PLUGIN(&p)

/** @} */
