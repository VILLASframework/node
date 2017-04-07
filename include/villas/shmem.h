#pragma once

/* The interface functions that the external program should use. */

#include "pool.h"
#include "queue.h"
#include "queue_signalled.h"

union shmem_queue {
	struct queue q;
	struct queue_signalled qs;
};

/** The structure that actually resides in the shared memory. TODO better name?*/
struct shmem_shared {
	union shmem_queue in; /**< Queue for samples passed from external program to node.*/
	union shmem_queue out; /**< Queue for samples passed from node to external program.*/
	struct pool pool; /**< Pool for the samples in the queues. */
};

struct shmem_shared * shmem_int_open(const char* name, size_t len);

size_t shmem_total_size(int insize, int outsize, int sample_size);
