#pragma once
/** Shared-memory interface: The interface functions that the external program should use.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/


#include "pool.h"
#include "queue.h"
#include "queue_signalled.h"

union shmem_queue {
	struct queue q;
	struct queue_signalled qs;
};

/** The structure that actually resides in the shared memory. */
struct shmem_shared {
	size_t len; /**< Total size of the shared memory region.*/
	union shmem_queue in; /**< Queue for samples passed from external program to node.*/
	int cond_in; /**< Whether to use a pthread_cond_t to signal if new samples are written to inqueue. */
	union shmem_queue out; /**< Queue for samples passed from node to external program.*/
	int cond_out; /**< Whether to use a pthread_cond_t to signal if new samples are written to outqueue. */
	struct pool pool; /**< Pool for the samples in the queues. */
	pthread_barrier_t start_bar;
	pthread_barrierattr_t start_attr;
	atomic_size_t node_stopped;
	atomic_size_t ext_stopped;
};

struct shmem_shared * shmem_shared_open(const char* name, void **base_ptr);

int shmem_shared_close(struct shmem_shared *shm, void *base);

int shmem_shared_read(struct shmem_shared *shm, struct sample *smps[], unsigned cnt);

int shmem_shared_write(struct shmem_shared *shm, struct sample *smps[], unsigned cnt);

size_t shmem_total_size(int insize, int outsize, int sample_size);
