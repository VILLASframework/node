/** Path destination
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

/** A path connects one input node to multiple output nodes (1-to-n).
 *
 * @addtogroup path Path
 * @{
 */

#pragma once

#include <villas/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct path;
struct sample;

struct path_destination {
	struct node *node;

	struct queue queue;
};

int path_destination_init(struct path_destination *pd, int queuelen);

int path_destination_destroy(struct path_destination *pd);

void path_destination_enqueue(struct path *p, struct sample *smps[], unsigned cnt);

void path_destination_write(struct path_destination *pd, struct path *p);

#ifdef __cplusplus
}
#endif

/** @} */
