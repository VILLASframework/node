/** Nodes.
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

#include <string.h>

#include "sample.h"
#include "node.h"
#include "super_node.h"
#include "utils.h"
#include "config.h"
#include "plugin.h"

int node_type_start(struct node_type *vt, struct super_node *sn)
{
	int ret;

	if (vt->state != STATE_DESTROYED)
		return 0;

	info("Initializing " YEL("%s") " node type which is used by %zu nodes", node_type_name(vt), list_length(&vt->instances));
	{ INDENT
		ret = vt->init ? vt->init(sn) : 0;
	}

	if (ret == 0)
		vt->state = STATE_STARTED;

	return ret;
}

int node_type_stop(struct node_type *vt)
{
	int ret;

	if (vt->state != STATE_STARTED)
		return 0;

	info("De-initializing " YEL("%s") " node type", node_type_name(vt));
	{ INDENT
		ret = vt->deinit ? vt->deinit() : 0;
	}

	if (ret == 0)
		vt->state = STATE_DESTROYED;

	return ret;
}

char * node_type_name(struct node_type *vt)
{
	return plugin_name(vt);
}