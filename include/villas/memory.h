/** Memory allocators.
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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <villas/node/config.h>
#include <villas/memory_type.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct node;

/** Descriptor of a memory block. Associated block always starts at
 * &m + sizeof(struct memory_block). */
struct memory_block {
	struct memory_block *prev;
	struct memory_block *next;
	size_t length; /**< Length of the block; doesn't include the descriptor itself */
	bool used;
};

/** @todo Unused for now */
struct memory_allocation {
	struct memory_type *type;

	struct memory_allocation *parent;

	void *address;
	size_t alignment;
	size_t length;

	union {
#ifdef IBVERBS_FOUND
		struct {
			struct ibv_mr *mr;
		} ib;
#endif
		struct {
			struct memory_block *block;
		} managed;
	};
};

/** Initilialize memory subsystem */
int memory_init(int hugepages);

int memory_lock(size_t lock);

/** Allocate \p len bytes memory of type \p m.
 *
 * @retval NULL If allocation failed.
 * @retval <>0  If allocation was successful.
 */
void * memory_alloc(struct memory_type *m, size_t len);

void * memory_alloc_aligned(struct memory_type *m, size_t len, size_t alignment);

int memory_free(void *ptr);

struct memory_allocation * memory_get_allocation(void *ptr);

#ifdef __cplusplus
}
#endif
