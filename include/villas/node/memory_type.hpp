/** Memory allocators.
 *
 * @file
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

#pragma once

#include <cstddef>
#include <cstdint>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

namespace memory {

struct Type;

typedef struct Allocation * (*allocator_t)(size_t len, size_t alignment, struct Type *mem);
typedef int (*deallocator_t)(struct Allocation * ma, struct Type *mem);

enum class Flags {
	MMAP		= (1 << 0),
	DMA		= (1 << 1),
	HUGEPAGE	= (1 << 2),
	HEAP		= (1 << 3)
};

struct Type {
	const char *name;
	int flags;

	size_t alignment;

	allocator_t alloc;
	deallocator_t free;

	void *_vd; /**< Virtual data for internal state */
};

extern struct Type heap;
extern struct Type mmap;
extern struct Type mmap_hugetlb;
extern struct Type *default_type;

struct Type * ib(NodeCompat *n, struct Type *parent);
struct Type * managed(void *ptr, size_t len);

int mmap_init(int hugepages) __attribute__ ((warn_unused_result));

} /* namespace memory */
} /* namespace node */
} /* namespace villas */
