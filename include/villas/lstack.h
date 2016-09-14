/** Lock-less LIFO stack
 *
 * Based on a single-linked list and double word compare exchange (DWCAS)
 * to solve the ABA problem.
 *
 * Based on https://github.com/skeeto/lstack
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#ifndef _LSTACK_H_
#define _LSTACK_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>

struct lstack_node {
	void *value;
	struct lstack_node *next;
};

/** DWCAS cmpexch16 */
struct lstack_head {
	uintptr_t aba;
	struct lstack_node *node;
};

struct lstack {
	struct lstack_node *node_buffer;
	_Atomic struct lstack_head head; /**> List of stack elements */
	_Atomic struct lstack_head free; /**> List of unused elements */
	_Atomic size_t size, avail;
};

/** Initialize a lock-less stack which can hold up to maxsz values.
 *
 * Note: this function is not thread-safe.
 */
int lstack_init(struct lstack *lstack, size_t maxsz);

/** Pop cnt values from the stack an place them in the array values */
ssize_t lstack_pop_many(struct lstack *lstack, void *values[], size_t cnt);

/** Push cnt values which are giving by the array values to the stack. */
ssize_t lstack_push_many(struct lstack *lstack, void *values[], size_t cnt);

/** Push a single value to the stack. */
static inline __attribute__((unused)) int lstack_push(struct lstack *lstack, void *value)
{
	return lstack_push_many(lstack, &value, 1) == 1 ? 0 : -1;
}

/** Pop a single element from the stack and return it. */
static inline __attribute__((unused)) void * lstack_pop(struct lstack *lstack)
{
	void *value;
	
	lstack_pop_many(lstack, &value, 1);
	
	return value;
}

/** Return the approximate size of the stack. */
static inline __attribute__((unused)) size_t lstack_size(struct lstack *lstack)
{
	return atomic_load(&lstack->size);
}

/** Release memory used by the stack.
 *
 * Note: this function is not thread-safe.
 */
static inline __attribute__((unused)) void lstack_destroy(struct lstack *lstack)
{
	free(lstack->node_buffer);
}

#endif /* _LSTACK_H_ */