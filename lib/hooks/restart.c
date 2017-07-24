/** Path restart hook.
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
#include "path.h"
#include "sample.h"

struct restart {
	struct sample *prev;
};

static int restart_start(struct hook *h)
{
	struct restart *r = h->_vd;

	r->prev = NULL;

	return 0;
}

static int restart_stop(struct hook *h)
{
	struct restart *r = h->_vd;

	if (r->prev)
		sample_put(r->prev);

	return 0;
}

static int restart_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	int i;
	struct restart *r = h->_vd;
	struct sample *prev, *cur = NULL;

	assert(h->path);

	for (i = 0, prev = r->prev; i < *cnt; i++, prev = cur) {
		cur = smps[i];

		if (prev) {
			if (cur->sequence == 0 && prev->sequence <= UINT32_MAX - 32) {
				warn("Simulation for path %s restarted (previous->seq=%u, current->seq=%u)",
					path_name(h->path), prev->sequence, cur->sequence);

				/* Run restart hooks */
				for (size_t i = 0; i < list_length(&h->path->hooks); i++) {
					struct hook *k = list_at(&h->path->hooks, i);

					hook_restart(k);
				}
			}
		}
	}

	if (cur)
		sample_get(cur);
	if (r->prev)
		sample_put(r->prev);

	r->prev = cur;

	return 0;
}

static struct plugin p = {
	.name		= "restart",
	.description	= "Call restart hooks for current path",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 1,
		.builtin = true,
		.read	= restart_read,
		.start	= restart_start,
		.stop	= restart_stop,
		.size	= sizeof(struct restart)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
