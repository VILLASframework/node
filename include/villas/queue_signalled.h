#pragma once

#include <pthread.h>

#include "queue.h"

/** Wrapper around queue that uses POSIX CV's for signalling writes. */
struct queue_signalled {
	struct queue q; /**< Actual underlying queue. */
	pthread_cond_t ready; /**< Condition variable to signal writes to the queue. */
	pthread_condattr_t readyattr;
	pthread_mutex_t mt; /**< Mutex for ready. */
	pthread_mutexattr_t mtattr;
};

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memtype *mem);

int queue_signalled_destroy(struct queue_signalled *qs);

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt);

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt);
