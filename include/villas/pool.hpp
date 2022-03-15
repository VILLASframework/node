/** Memory pool for fixed size objects.
 *
 * This datastructure is based on a lock-less stack (lstack).
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

#include <cstddef>
#include <sys/types.h>

#include <villas/queue.h>
#include <villas/common.hpp>
#include <villas/node/memory.hpp>

namespace villas {
namespace node {

/** A thread-safe memory pool */
struct Pool {
	enum State state;

	off_t  buffer_off; /**< Offset from the struct address to the underlying memory area */

	size_t len;		/**< Length of the underlying memory area */
	size_t blocksz;		/**< Length of a block in bytes */
	size_t alignment;	/**< Alignment of a block in bytes */

	struct CQueue queue; /**< The queue which is used to keep track of free blocks */
};

#define INLINE static inline __attribute__((unused))
#define pool_buffer(p) ((char *) (p) + (p)->buffer_off)

/** Initiazlize a pool
 *
 * @param[inout] p The pool data structure.
 * @param[in] cnt The total number of blocks which are reserverd by this pool.
 * @param[in] blocksz The size in bytes per block.
 * @param[in] mem The type of memory which should be used for this pool.
 * @retval 0 The pool has been successfully initialized.
 * @retval <>0 There was an error during the pool initialization.
 */
int pool_init(struct Pool *p, size_t cnt, size_t blocksz, struct memory::Type *mem = memory::default_type) __attribute__ ((warn_unused_result));

/** Destroy and release memory used by pool. */
int pool_destroy(struct Pool *p) __attribute__ ((warn_unused_result));

/** Pop up to \p cnt values from the stack an place them in the array \p blocks.
 *
 * @return The number of blocks actually retrieved from the pool.
 *         This number can be smaller than the requested \p cnt blocks
 *         in case the pool currently holds less than \p cnt blocks.
 */
INLINE ssize_t pool_get_many(struct Pool *p, void *blocks[], size_t cnt)
{
	return queue_pull_many(&p->queue, blocks, cnt);
}

/** Push \p cnt values which are giving by the array values to the stack. */
INLINE ssize_t pool_put_many(struct Pool *p, void *blocks[], size_t cnt)
{
	return queue_push_many(&p->queue, blocks, cnt);
}

/** Get a free memory block from pool. */
INLINE void * pool_get(struct Pool *p)
{
	void *ptr;
	return queue_pull(&p->queue, &ptr) == 1 ? ptr : nullptr;
}

/** Release a memory block back to the pool. */
INLINE int pool_put(struct Pool *p, void *buf)
{
	return queue_push(&p->queue, buf);
}

} /* namespace node */
} /* namespace villas */
