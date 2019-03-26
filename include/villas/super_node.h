/** The super node object holding the state of the application (C-compatability header).
 *
 * @file
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

#pragma once

#include <villas/config.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct vlist;
struct web;
struct super_node;
struct lws;

struct vlist * super_node_get_nodes(struct super_node *sn);

struct vlist * super_node_get_paths(struct super_node *sn);

struct vlist * super_node_get_interfaces(struct super_node *sn);

int super_node_get_cli_argc(struct super_node *sn);

struct web * super_node_get_web(struct super_node *sn);

struct lws_context * web_get_context(struct web *w);

struct lws_vhost * web_get_vhost(struct web *w);

enum state web_get_state(struct web *w);

#ifdef WITH_WEB
int web_callback_on_writable(struct web *w, struct lws *wsi);
#endif

#ifdef __cplusplus
}
#endif
