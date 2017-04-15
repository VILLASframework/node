/** Wrapper around queue that uses POSIX CV's for signalling writes.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <pthread.h>

#include "queue.h"

/** Wrapper around queue that uses POSIX CV's for signalling writes. */
struct queue_signalled {
	struct queue queue;		/**< Actual underlying queue. */
	pthread_cond_t ready;		/**< Condition variable to signal writes to the queue. */
	pthread_mutex_t mutex;		/**< Mutex for ready. */
};

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memtype *mem);

int queue_signalled_destroy(struct queue_signalled *qs);

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt);

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt);
