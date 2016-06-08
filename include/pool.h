/** Memory pool for fixed size objects.
 *
 * This datastructure is based on a lock-less stack (lstack).
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#ifndef _POOL_H_
#define _POOL_H_

#include "lstack.h"

struct pool_block;

struct pool {
	size_t blocksz;
	size_t alignment;
	
	struct lstack stack;
};

/** Initiazlize a pool */
int pool_init(struct pool *p, size_t blocksz, size_t alignment, void *buf, size_t len);

/** Allocate hugepages for the pool and initialize it */
int pool_init_mmap(struct pool *p, size_t blocksz, size_t cnt);

/** Destroy and release memory used by pool. */
static inline __attribute__((unused)) void pool_destroy(struct pool *p)
{
	lstack_destroy(&p->stack);
}

/** Pop cnt values from the stack an place them in the array blocks */
static inline __attribute__((unused)) ssize_t pool_get_many(struct pool *p, void *blocks[], size_t cnt)
{
	return lstack_pop_many(&p->stack, blocks, cnt);
}

/** Push cnt values which are giving by the array values to the stack. */
static inline __attribute__((unused)) ssize_t pool_put_many(struct pool *p, void *blocks[], size_t cnt)
{
	return lstack_push_many(&p->stack, blocks, cnt);
}

/** Get a free memory block from pool. */
static inline __attribute__((unused)) void * pool_get(struct pool *p)
{
	return lstack_pop(&p->stack);
}

/** Release a memory block back to the pool. */
static inline __attribute__((unused)) int pool_put(struct pool *p, void *buf)
{
	return lstack_push(&p->stack, buf);
}

#endif /* _POOL_H_ */