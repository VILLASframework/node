/** Memory pool for fixed size objects.
 *
 * This datastructure is based on a lock-less stack (lstack).
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#ifndef _POOL_H_
#define _POOL_H_

#include <sys/types.h>

#include "queue.h"

struct memtype;

/** A thread-safe memory pool */
struct pool {
	void *buffer;		/**< Address of the underlying memory area */
	const struct memtype *mem;
	
	size_t len;		/**< Length of the underlying memory area */
	
	size_t blocksz;		/**< Length of a block in bytes */
	size_t alignment;	/**< Alignment of a block in bytes */
	
	struct queue queue; /**< The queue which is used to keep track of free blocks */
};

#define INLINE static inline __attribute__((unused)) 

/** Initiazlize a pool */
int pool_init(struct pool *p, size_t blocksz, size_t alignment, const struct memtype *mem);

/** Destroy and release memory used by pool. */
int pool_destroy(struct pool *p);

/** Pop cnt values from the stack an place them in the array blocks */
INLINE ssize_t pool_get_many(struct pool *p, void *blocks[], size_t cnt)
{
	return queue_pull_many(&p->queue, blocks, cnt);
}

/** Push cnt values which are giving by the array values to the stack. */
INLINE ssize_t pool_put_many(struct pool *p, void *blocks[], size_t cnt)
{
	return queue_push_many(&p->queue, blocks, cnt);
}

/** Get a free memory block from pool. */
INLINE void * pool_get(struct pool *p)
{
	void *ptr;
	return queue_pull(&p->queue, &ptr) == 1 ? ptr : NULL;
}

/** Release a memory block back to the pool. */
INLINE int pool_put(struct pool *p, void *buf)
{
	return queue_push(&p->queue, buf);
}

#endif /* _POOL_H_ */