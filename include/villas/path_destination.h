/** Path destination
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

/** A path connects one input node to multiple output nodes (1-to-n).
 *
 * @addtogroup path Path
 * @{
 */

#pragma once

#include <villas/queue.h>
#include <villas/log.hpp>

/* Forward declarations */
struct vpath;
struct sample;

struct vpath_destination {
	villas::Logger logger;

	struct vnode *node;
	struct vpath *path;

	struct queue queue;
};

int path_destination_init(struct vpath_destination *pd, struct vpath *p, struct vnode *n) __attribute__ ((warn_unused_result));

int path_destination_destroy(struct vpath_destination *pd) __attribute__ ((warn_unused_result));

int path_destination_prepare(struct vpath_destination *pd, int queuelen);

void path_destination_check(struct vpath_destination *pd);

void path_destination_enqueue(struct vpath *p, const struct sample * const smps[], unsigned cnt);

void path_destination_write(struct vpath_destination *pd, struct vpath *p);

/** @} */
