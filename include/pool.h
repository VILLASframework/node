/** Circular buffer
 *
 * Every path uses a circular buffer to save past messages.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */

#ifndef _POOL_H_
#define _POOL_H_

#define pool_start(pool)	pool_get(pool, 0)

#define pool_end(pool)		pool_get(pool, pool->lenght)

/** Return pointer to last element which has been inserted. */
#define pool_current(pool)	pool_getrel(pool,  0)

/** Return pointer to the element before the previously added one. */
#define pool_previous(pool)	pool_getrel(pool, -1)

/** Iterate through the circuluar buffer. */
#define pool_foreach(it, pool, start, end) for (it  = pool_get(pool, start);	\
						it != pool_get(pool, end);	\
						it  = pool_get_next(pool, it))

/** Return the number of elements in the pool. */
#define pool_length(pool)	((pool)->length)

/** Return the stride between two consecutive elemements in the pool. */
#define pool_stride(pool)	((pool)->stride)

/** The data structure for a message pool.
 *
 * A message pool is a circular buffer used to store messages (samples)
 */
struct pool {
	void *buffer;		/**< Heap allocated memory of the circular buffer. */
	
	int last;		/**< Index of the message which has been added last. */
	int previous;		/**< Previous value of pool::last. */
	
	size_t length;		/**< Number of messages in the circular buffer */
	size_t stride;		/**< Size per block in bytes. */
};

/** Initiliaze a new pool and allocate memory. */
void pool_create(struct pool *p, size_t len, size_t blocklen);

/** Release memory of pool. */
void pool_destroy(struct pool *p);

/** Advance the internal pointer of the pool. */
void pool_push(struct pool *p, int blocks);

/** Return pointer to pool element. */
void * pool_get(struct pool *p, int index);

/** Return pointer relative to last inserted element. */
void * pool_getrel(struct pool *p, int offset);

/** Return next element after ptr */
void * pool_get_next(struct pool *p, void *ptr);

/** Return element before ptr */
void * pool_get_previous(struct pool *p, void *ptr);


#endif /* _POOL_H_ */