
/** Dump hook.
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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/node.h>
#include <villas/sample.h>

static int dump_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	for (int i = 0; i < *cnt; i++)
		sample_dump(smps[i]);

	return 0;
}

static struct plugin p = {
	.name		= "dump",
	.description	= "dump data to stdout",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ | HOOK_NODE_WRITE | HOOK_PATH,
		.priority	= 1,
		.process	= dump_process
	}
};

REGISTER_PLUGIN(&p)

/** @} */
