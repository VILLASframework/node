/* Memory pool for fixed size objects.
 *
 * This datastructure is based on a lock-less stack (lstack).
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <sys/types.h>

#include <villas/queue.h>
#include <villas/common.hpp>
#include <villas/node/memory.hpp>

namespace villas {
namespace node {

// A thread-safe memory pool
struct Pool {
	enum State state;

	off_t  buffer_off; // Offset from the struct address to the underlying memory area

	size_t len;		// Length of the underlying memory area
	size_t blocksz;		// Length of a block in bytes
	size_t alignment;	// Alignment of a block in bytes

	struct CQueue queue; // The queue which is used to keep track of free blocks
};

#define pool_buffer(p) ((char *) (p) + (p)->buffer_off)

/* Initiazlize a pool
 *
 * @param[inout] p The pool data structure.
 * @param[in] cnt The total number of blocks which are reserverd by this pool.
 * @param[in] blocksz The size in bytes per block.
 * @param[in] mem The type of memory which should be used for this pool.
 * @retval 0 The pool has been successfully initialized.
 * @retval <>0 There was an error during the pool initialization.
 */
int pool_init(struct Pool *p, size_t cnt, size_t blocksz, struct memory::Type *mem = memory::default_type) __attribute__ ((warn_unused_result));

// Destroy and release memory used by pool.
int pool_destroy(struct Pool *p) __attribute__ ((warn_unused_result));

/* Pop up to \p cnt values from the stack an place them in the array \p blocks.
 *
 * @return The number of blocks actually retrieved from the pool.
 *         This number can be smaller than the requested \p cnt blocks
 *         in case the pool currently holds less than \p cnt blocks.
 */
ssize_t pool_get_many(struct Pool *p, void *blocks[], size_t cnt);

// Push \p cnt values which are giving by the array values to the stack.
ssize_t pool_put_many(struct Pool *p, void *blocks[], size_t cnt);

// Get a free memory block from pool.
void * pool_get(struct Pool *p);

// Release a memory block back to the pool.
int pool_put(struct Pool *p, void *buf);

} // namespace node
} // namespace villas
