/** Drop hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <inttypes.h>

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/stats.h>
#include <villas/node.h>
#include <villas/sample.h>

struct drop {
	struct sample *prev;
};

static int drop_start(struct hook *h)
{
	struct drop *d = (struct drop *) h->_vd;

	d->prev = NULL;

	return 0;
}

static int drop_stop(struct hook *h)
{
	struct drop *d = (struct drop *) h->_vd;

	if (d->prev)
		sample_decref(d->prev);

	return 0;
}

static int drop_process(struct hook *h, struct sample *smp)
{
	int dist;

	struct drop *d = (struct drop *) h->_vd;


	if (d->prev) {
		dist = smp->sequence - (int64_t) d->prev->sequence;
		if (dist <= 0) {
			debug(10, "Dropping reordered sample: sequence=%" PRIu64 ", distance=%d", smp->sequence, dist);

			return HOOK_SKIP_SAMPLE;
		}
	}

	sample_incref(smp);
	if (d->prev)
		sample_decref(d->prev);

	d->prev = smp;

	return HOOK_OK;
}

static int drop_restart(struct hook *h)
{
	struct drop *d = (struct drop *) h->_vd;

	if (d->prev) {
		sample_decref(d->prev);
		d->prev = NULL;
	}

	return 0;
}

static struct plugin p = {
	.name		= "drop",
	.description	= "Drop messages with reordered sequence numbers",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_BUILTIN | HOOK_NODE_READ,
		.priority	= 3,
		.process	= drop_process,
		.start		= drop_start,
		.stop		= drop_stop,
		.restart	= drop_restart,
		.size		= sizeof(struct drop)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
