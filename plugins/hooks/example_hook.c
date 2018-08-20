/** A simple example hook function which can be loaded as a plugin.
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

#include <stddef.h>

#include <villas/hook.h>
#include <villas/log.h>
#include <villas/plugin.h>

struct hook;
struct path;
struct sample;

static int example_start(struct hook *h)
{
	info("Hello world from example hook!");

	return 0;
}

static struct plugin p = {
	.name		= "example",
	.description	= "This is just a simple example hook",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.start	= example_start
	}
};

REGISTER_PLUGIN(&p)
