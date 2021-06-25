/** Node direction
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @addtogroup node Node
 * @{
 */

#pragma once

#include <jansson.h>

#include <villas/common.hpp>
#include <villas/list.h>

/* Forward declarations */
struct vnode;
struct vpath;

enum class NodeDir {
	IN,			/**< VILLASnode is receiving/reading */
	OUT			/**< VILLASnode is sending/writing */
};

struct vnode_direction {
	enum State state;
	enum NodeDir direction;

	/** The path which uses this node as a source/destination.
	 *
	 * Usually every node should be used only by a single path as destination.
	 * Otherwise samples from different paths would be interleaved.
	 */
	struct vpath *path;

	bool enabled;		/**< This node direction is enabled. */
	bool builtin_hooks;	/**< This node direction should use built-in hooks by default. */
	bool read_only_hooks;	/**< The hooks of this node direction do not alter samples while processing them. */

	unsigned vectorize;	/**< Number of messages to send / recv at once (scatter / gather) */

	struct vlist hooks;	/**< List of read / write hooks (struct hook). */
	struct vlist signals;	/**< Signal description. */

	json_t *config;		/**< A JSON object containing the configuration of the node. */
};

int node_direction_init(struct vnode_direction *nd, enum NodeDir dir, struct vnode *n) __attribute__ ((warn_unused_result));

int node_direction_destroy(struct vnode_direction *nd, struct vnode *n) __attribute__ ((warn_unused_result));

int node_direction_parse(struct vnode_direction *nd, struct vnode *n, json_t *json);

int node_direction_check(struct vnode_direction *nd, struct vnode *n);

int node_direction_prepare(struct vnode_direction *nd, struct vnode *n);

int node_direction_start(struct vnode_direction *nd, struct vnode *n);

int node_direction_stop(struct vnode_direction *nd, struct vnode *n);

struct vlist * node_direction_get_signals(struct vnode_direction *nd);

/** @} */
