/** Lock-less LIFO stack
 *
 * Based on a single-linked list and double word compare exchange (DWCAS)
 * to solve the ABA problem.
 *
 * Based on https://github.com/skeeto/lstack
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <errno.h>

#include "lstack.h"
#include "utils.h"

static struct lstack_node * pop_many(_Atomic struct lstack_head *head, size_t cnt)
{
	size_t i;
	struct lstack_head next, orig = atomic_load(head);

	do {
		next.aba  = orig.aba + 1;
		
		for (i = 0,	next.node = orig.node;
		     i < cnt &&	next.node;
		     i++,	next.node = next.node->next) {
//			     debug(3, "pop_many next.node %p next.node->next %p", next.node, next.node->next);
		}
		
		if (i != cnt)
			return NULL;
	} while (!atomic_compare_exchange_weak(head, &orig, next));

	return orig.node;
}

static int push_many(_Atomic struct lstack_head *head, struct lstack_node *node, size_t cnt)
{
	size_t i;
	struct lstack_head next, orig = atomic_load(head);
	struct lstack_node *last = node;
	
	/* Find last node which will be pushed */
	for (i = 1; i < cnt; i++) {
//		debug(3, "push_many node %p node->next %p", last, last->next);
		last = last->next;
	}

	do {
		next.aba  = orig.aba + 1;
		next.node = node;

		last->next = orig.node;
	} while (!atomic_compare_exchange_weak(head, &orig, next));
	
	return 0;
}

int lstack_init(struct lstack *lstack, size_t maxsz)
{
	/* Pre-allocate all nodes. */
	lstack->node_buffer = alloc(maxsz * sizeof(struct lstack_node));

	for (size_t i = 1; i < maxsz; i++)
		lstack->node_buffer[i-1].next = &lstack->node_buffer[i];
	lstack->node_buffer[maxsz - 1].next = NULL;


	lstack->free = ATOMIC_VAR_INIT(((struct lstack_head) { 0, lstack->node_buffer }));
	lstack->head = ATOMIC_VAR_INIT(((struct lstack_head) { 0, NULL }));

	lstack->size = ATOMIC_VAR_INIT(0);
	lstack->avail = ATOMIC_VAR_INIT(maxsz);

	return 0;
}

ssize_t lstack_push_many(struct lstack *lstack, void *values[], size_t cnt)
{
	size_t i;
	struct lstack_node *nodes, *node;
	
	if (cnt == 0)
		return 0;

	nodes = pop_many(&lstack->free, cnt);
	if (!nodes)
		return 0;

	atomic_fetch_sub(&lstack->avail, cnt);

	for (i = 0,	node = nodes;
	     i < cnt &&	node;
	     i++,	node = node->next)
		node->value = values[i];
	
	push_many(&lstack->head, nodes, cnt);
	atomic_fetch_add(&lstack->size, cnt);

	return i;
}

ssize_t lstack_pop_many(struct lstack *lstack, void *values[], size_t cnt)
{
	size_t i;
	struct lstack_node *nodes, *node;
	
	if (cnt == 0)
		return 0;

	nodes = pop_many(&lstack->head, cnt);
	if (!nodes)
		return 0;
	
	atomic_fetch_sub(&lstack->size, cnt);
	
	for (i = 0,	node = nodes;
	     i < cnt &&	node;
	     i++,	node = node->next)
		values[i] = node->value;

	push_many(&lstack->free, nodes, cnt);
	atomic_fetch_add(&lstack->avail, cnt);

	return i;
}