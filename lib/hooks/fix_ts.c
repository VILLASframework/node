/** Fix timestamp hook.
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
#include "timing.h"
#include "sample.h"

int fix_ts_read(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct timespec now;

	now = time_now();

	for (int i = 0; i < *cnt; i++) {
		/* Check for missing receive timestamp
		 * Usually node_type::read() should update the receive timestamp.
		 * An example would be to use hardware timestamp capabilities of
		 * modern NICs.
		 */
		if ((smps[i]->ts.received.tv_sec ==  0 && smps[i]->ts.received.tv_nsec ==  0) ||
		    (smps[i]->ts.received.tv_sec == -1 && smps[i]->ts.received.tv_nsec == -1))
			smps[i]->ts.received = now;

		/* Check for missing origin timestamp */
		if ((smps[i]->ts.origin.tv_sec ==  0 && smps[i]->ts.origin.tv_nsec ==  0) ||
		    (smps[i]->ts.origin.tv_sec == -1 && smps[i]->ts.origin.tv_nsec == -1))
			smps[i]->ts.origin = now;
	}

	return 0;
}

static struct plugin p = {
	.name		= "fix_ts",
	.description	= "Update timestamps of sample if not set",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags	= HOOK_NODE | HOOK_BUILTIN,
		.priority = 0,
		.read	= fix_ts_read,
	}
};

REGISTER_PLUGIN(&p)

/** @} */
