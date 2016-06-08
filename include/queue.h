/** Lock-free Single-Producer Single-consumer (SPSC) queue.
 *
 * This datastructure queues void pointers in a FIFO style.
 * Every queue element has an associated reference count which is
 * used to tell wheater the queue is full or empty.
 *
 * Queue head and tail pointers are not part of this datastructure.
 * In combination with the reference counting, this enables multiple
 * readers each with its own head pointer.
 * Each reader will read the same data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

typedef uintmax_t qptr_t;

struct queue {
	        size_t size;	/**< Number of pointers in queue::array */
	_Atomic int *refcnts;	/**< Reference counts to blocks in queue::array */
	_Atomic int  readers;	/**< Initial reference count for pushed blocks */

	void **pointers;	/**< Pointers to queue data */
};

/** Initiliaze a new queue and allocate memory. */
int queue_init(struct queue *q, size_t size);

/** Release memory of queue. */
void queue_destroy(struct queue *q);

/** Increment the number of readers for this queue.
 *
 * Important: To garuantee thread-safety this function must by called by
 *            the (only) writer which holds \p tail.
 */
void queue_reader_add(struct queue *q, qptr_t head, qptr_t tail);

/** Decrement the number of readers for this queue.
 *
 * Important: To garuantee thread-safety this function must by called by
 *            the (only) writer which holds \p tail.
 */
void queue_reader_remove(struct queue *q, qptr_t head, qptr_t tail);

/** Enqueue up to \p cnt elements from \p ptrs[] at the queue tail pointed by \p tail.
 *
 * It may happen that the queue is (nearly) full and there is no more
 * space to enqueue more elments.
 * In this case a call to this function will return a value which is smaller than \p cnt
 * or even zero if the queue was already full.
 *
 * @param q A pointer to the queue datastructure.
 * @param[in] ptrs An array of void-pointers which should be enqueued.
 * @param cnt The length of the pointer array \p ptrs.
 * @param[in,out] tail A pointer to the current tail of the queue. The tail will be updated to new tail after eqeuing.
 * @return The function returns the number of successfully enqueued elements from \p ptrs.
 */
int queue_push_many(struct queue *q, void *ptrs[], size_t cnt, qptr_t *tail);

/** Dequeue up to \p cnt elements from the queue and place them into the array \p ptrs[].
 *
 * @param q A pointer to the queue datastructure.
 * @param[out] ptrs An array with space at least \cnt elements which will receive pointers to the released elements.
 * @param cnt The maximum number of elements which should be dequeued. It defines the size of \p ptrs.
 * @param[in,out] head A pointer to a queue head. The value will be updated to reflect the new head.
 * @return The number of elements which have been dequeued <b>and whose reference counts have reached zero</b>.
 */
int queue_pull_many(struct queue *q, void *ptrs[], size_t cnt, qptr_t *head);

/** Fill \p ptrs with \p cnt elements of the queue starting at entry \p pos. */
int queue_get_many(struct queue *q, void *ptrs[], size_t cnt, qptr_t pos);


int queue_get(struct queue *q, void **ptr, qptr_t pos);

/** Enqueue a new block at the tail of the queue which is given by the qptr_t. */
int queue_push(struct queue *q, void *ptr, qptr_t *tail);

/** Dequeue the first block at the head of the queue. */
int queue_pull(struct queue *q, void **ptr, qptr_t *head);

#endif /* _QUEUE_H_ */