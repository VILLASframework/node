/** The "restart" API ressource.
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

#include "plugin.h"
#include "api.h"

/** @todo not implemented yet */
static int api_restart(struct api_action *h, json_t *args, json_t **resp, struct api_session *s)
{
	return -1;
}

static struct plugin p = {
	.name = "restart",
	.description = "restart VILLASnode with new configuration",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_restart
};

REGISTER_PLUGIN(&p)
