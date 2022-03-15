/** Bridge code for call C-function pointers from Go code
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdlib.h>

#include <villas/nodes/go.h>

void bridge_go_register_node_factory(_go_register_node_factory_cb cb, _go_plugin_list pl, char *name, char *desc, int flags)
{
    cb(pl, name, desc, flags);
}

void bridge_go_logger_log(_go_logger_log_cb cb, _go_logger l, int level, char *msg)
{
    cb(l, level, msg);
    free(msg);
}
