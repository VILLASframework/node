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

#include <inttypes.h>

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/node.h>
#include <villas/sample.h>

struct restart {
	struct sample *prev;
};

static int restart_start(struct hook *h)
{
	struct restart *r = (struct restart *) h->_vd;

	r->prev = NULL;

	return 0;
}

static int restart_stop(struct hook *h)
{
	struct restart *r = (struct restart *) h->_vd;

	if (r->prev)
		sample_decref(r->prev);

	return 0;
}

static int restart_read(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	unsigned i;
	struct restart *r = (struct restart *) h->_vd;
	struct sample *prev, *cur = NULL;

	assert(h->node);

	for (i = 0, prev = r->prev; i < *cnt; i++, prev = cur) {
		cur = smps[i];

		if (prev) {
			/* A wrap around of the sequence no should not be treated as a simulation restart */
			if (cur->sequence == 0 && prev->sequence <= (int) (UINT32_MAX - 128)) {
				warn("Simulation from node %s restarted (previous->sequence=%" PRIu64 ", current->sequence=%" PRIu64 ")",
					node_name(h->node), prev->sequence, cur->sequence);

				cur->flags |= SAMPLE_IS_FIRST;

				/* Run restart hooks */
				for (size_t i = 0; i < list_length(&h->node->in.hooks); i++) {
					struct hook *k = (struct hook *) list_at(&h->node->in.hooks, i);

					hook_restart(k);
				}

				for (size_t i = 0; i < list_length(&h->node->out.hooks); i++) {
					struct hook *k = (struct hook *) list_at(&h->node->out.hooks, i);

					hook_restart(k);
				}
			}
		}
	}

	if (cur)
		sample_incref(cur);
	if (r->prev)
		sample_decref(r->prev);

	r->prev = cur;

	return 0;
}

static struct plugin p = {
	.name		= "restart",
	.description	= "Call restart hooks for current node",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags	= HOOK_NODE | HOOK_BUILTIN,
		.priority = 1,
		.read	= restart_read,
		.start	= restart_start,
		.stop	= restart_stop,
		.size	= sizeof(struct restart)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
