/** Memory allocators.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#ifdef IBVERBS_FOUND
  #include <infiniband/verbs.h>
#endif /* IBVERBS_FOUND */

#include <cstddef>
#include <cstdint>

#include <villas/node/config.hpp>
#include <villas/node/memory_type.hpp>

namespace villas {
namespace node {
namespace memory {

/** Descriptor of a memory block. Associated block always starts at
 * &m + sizeof(struct Block). */
struct Block {
	struct Block *prev;
	struct Block *next;
	size_t length; /**< Length of the block; doesn't include the descriptor itself */
	bool used;
};

/** @todo Unused for now */
struct Allocation {
	struct Type *type;

	struct Allocation *parent;

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
			struct Block *block;
		} managed;
	};
};

/** Initilialize memory subsystem */
int init(int hugepages) __attribute__ ((warn_unused_result));

int lock(size_t lock);

/** Allocate \p len bytes memory of type \p m.
 *
 * @retval nullptr If allocation failed.
 * @retval <>0  If allocation was successful.
 */
void * alloc(size_t len, struct Type *m = default_type);

void * alloc_aligned(size_t len, size_t alignment, struct Type *m = default_type);

int free(void *ptr);

struct Allocation * get_allocation(void *ptr);

} /* namespace memory */
} /* namespace node */
} /* namespace villas */
