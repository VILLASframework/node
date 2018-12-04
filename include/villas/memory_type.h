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

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct memory_type;
struct node;

typedef struct memory_allocation * (*memory_allocator_t)(struct memory_type *mem, size_t len, size_t alignment);
typedef int (*memory_deallocator_t)(struct memory_type *mem, struct memory_allocation * ma);

enum memory_type_flags {
	MEMORY_MMAP	= (1 << 0),
	MEMORY_DMA	= (1 << 1),
	MEMORY_HUGEPAGE	= (1 << 2),
	MEMORY_HEAP	= (1 << 3)
};

struct memory_type {
	const char *name;
	int flags;

	size_t alignment;

	memory_allocator_t alloc;
	memory_deallocator_t free;

	void *_vd; /**< Virtual data for internal state */
};

extern struct memory_type memory_heap;
extern struct memory_type memory_hugepage;

struct memory_type * memory_ib(struct node *n, struct memory_type *parent);
struct memory_type * memory_managed(void *ptr, size_t len);

int memory_hugepage_init(int hugepages);

struct memory_type * memory_type_lookup(enum memory_type_flags flags);

#ifdef __cplusplus
}
#endif
