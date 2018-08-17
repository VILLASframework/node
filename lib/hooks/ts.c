/** Timestamp hook.
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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/timing.h>
#include <villas/sample.h>

static int ts_read(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	for (int i = 0; i < *cnt; i++)
		smps[i]->ts.origin = smps[i]->ts.received;

	return 0;
}

static struct plugin p = {
	.name		= "ts",
	.description	= "Overwrite origin timestamp of samples with receive timestamp",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE,
		.priority	= 99,
		.read		= ts_read
	}
};

REGISTER_PLUGIN(&p)

/** @} */
