/** The super node object holding the state of the application (C-compatability header).
 *
 * @file
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

struct list;
struct super_node;

struct list * super_node_get_nodes(struct super_node *sn);

struct list * super_node_get_nodes(struct super_node *sn);

struct lws_context * super_node_get_web_context(struct super_node *sn);

struct lws_vhost * super_node_get_web_vhost(struct super_node *sn);

enum state super_node_get_web_state(struct super_node *sn);

int super_node_get_cli_argc(struct super_node *sn);
