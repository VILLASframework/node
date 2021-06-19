/** Message source
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

#include <villas/pool.h>
#include <villas/list.h>

/* Forward declarations */
struct vpath;
struct sample;

enum PathSourceType {
	MASTER,
	SECONDARY
};

struct vpath_source {
	struct vnode *node;
	struct vpath *path;

	bool masked;

	enum PathSourceType type;

	struct pool pool;
	struct vlist mappings;			/**< List of mappings (struct mapping_entry). */
	struct vlist secondaries;		/**< List of secondary path sources (struct path_sourced). */
};

int path_source_init_master(struct vpath_source *ps, struct vpath *p, struct vnode *n) __attribute__ ((warn_unused_result));

int path_source_init_secondary(struct vpath_source *ps, struct vpath *p, struct vnode *n) __attribute__ ((warn_unused_result));

int path_source_destroy(struct vpath_source *ps) __attribute__ ((warn_unused_result));

void path_source_check(struct vpath_source *ps);

int path_source_read(struct vpath_source *ps, int i);

/** @} */
