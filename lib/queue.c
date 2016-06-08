/** Lock-free Single-Producer Single-consumer (SPSC) queue.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */

#include "queue.h"
#include "utils.h"

int queue_init(struct queue *q, size_t size)
{
	q->size = size;

	q->pointers = alloc(size * sizeof(void *));
	q->refcnts  = alloc(size * sizeof(atomic_uint));
	q->readers  = 0;

	return 0;
}

void queue_destroy(struct queue *q)
{
	free(q->pointers);
	free(q->refcnts);
}

void queue_reader_add(struct queue *q, qptr_t head, qptr_t tail)
{
	assert (head <= tail);

	atomic_fetch_add(&q->readers, 1);
	
	for (qptr_t i = head; i < tail; i++)
		atomic_fetch_add(&q->refcnts[i % q->size], 1);
}

void queue_reader_remove(struct queue *q, qptr_t head, qptr_t tail)
{
	assert (head <= tail);

	atomic_fetch_sub(&q->readers, 1);
	
	for (qptr_t i = head; i < tail; i++)
		atomic_fetch_sub(&q->refcnts[i % q->size], 1);
}

int queue_get(struct queue *q, void **ptr,  qptr_t pos)
{
	int refcnt = atomic_load(&q->refcnts[pos % q->size]);
	if (refcnt == 0)
		return 0;
	
	*ptr = q->pointers[pos % q->size];

	return 1;
}

int queue_push(struct queue *q, void *ptr, qptr_t *tail)
{
	int refcnt;

	do {
		refcnt = atomic_load(&q->refcnts[*tail % q->size]);
		if (refcnt != 0)
			return 0; /* Queue is full */
		
		q->pointers[*tail % q->size] = ptr;
	} while (!atomic_compare_exchange_weak(&q->refcnts[*tail % q->size], &refcnt, q->readers));

	*tail = *tail + 1;
	
	return 1;
}

int queue_pull(struct queue *q, void **ptr, qptr_t *head)
{
	int refcnt;

	do {
		refcnt = atomic_load(&q->refcnts[*head % q->size]);
		if (refcnt == 0)
			return -1; /* Queue is empty */
		
		*ptr = q->pointers[*head % q->size];
	} while (!atomic_compare_exchange_weak(&q->refcnts[*head % q->size], &refcnt, refcnt - 1));
	
	*head = *head + 1;
	
	return refcnt == 1 ? 1 : 0;
}

int queue_get_many(struct queue *q, void *ptrs[], size_t cnt, qptr_t pos)
{
	int ret, i;

	for (i = 0; i < cnt; i++) {
		ret = queue_get(q, &ptrs[i], pos + i);
		if (ret == 0)
			break;
	}

	return i;
}

int queue_push_many(struct queue *q, void **ptrs, size_t cnt, qptr_t *tail)
{
	int ret, i;

	for (i = 0; i < cnt; i++) {
		ret = queue_push(q, ptrs[i], tail);
		if (ret == 0)
			break;
	}

	return i;
}

int queue_pull_many(struct queue *q, void **ptrs, size_t cnt, qptr_t *head)
{
	int released = 0, ret;
	
	for (int i = 0; i < cnt; i++) {
		ret = queue_pull(q, &ptrs[i], head);
		if (ret < 0) /* empty */
			break;
		if (ret == 1)
			released++;
	}

	return released;
}