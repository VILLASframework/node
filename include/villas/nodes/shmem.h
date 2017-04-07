#ifndef _SHMEM_H_
#define _SHMEM_H_

#include "node.h"
#include "memory.h"
#include "pool.h"
#include "queue.h"
#include "queue_signalled.h"

#define DEFAULT_SHMEM_QUEUESIZE 512

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

struct shmem {
	const char* name; /**< Name of the shm object. */
	int sample_size; /**< Number of data entries for each sample. */
	int insize, outsize; /**< Size of ingoing and outgoing queue, respectively. */
	int cond_out; /**< Whether to use a pthread_cond_t to signal if new samples are written to outqueue. */
	int cond_in; /**< Whether to use a pthread_cond_t to signal if new samples are written to inqueue. */

	size_t len; /**< Overall size of shared memory object. */
	struct memtype *manager; /**< Manager for the shared memory region. */
	int fd; /**< Handle as returned by shm_open().*/
	void *base; /**< Pointer to the shared memory region. */
	struct shmem_shared *shared; /**< Shared datastructure. */
};

char *shmem_print(struct node *n);

int shmem_parse(struct node *n, config_setting_t *cfg);

int shmem_open(struct node *n);

int shmem_close(struct node *n);

int shmem_read(struct node *n, struct sample *smps[], unsigned cnt);

int shmem_write(struct node *n, struct sample *smps[], unsigned cnt);

/* The interface functions that the external program should use. TODO put this
 * in another file? */

struct shmem_shared * shmem_int_open(const char* name, size_t len);

size_t shmem_total_size(int insize, int outsize, int sample_size);
#endif /* _SHMEM_H_ */
