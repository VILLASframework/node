/** Shared-memory interface: The interface functions that the external program should use.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "kernel/kernel.h"
#include "memory.h"
#include "utils.h"
#include "sample.h"
#include "shmem.h"

size_t shmem_total_size(int insize, int outsize, int sample_size)
{
	/* We have the constant const of the memtype header */
	return sizeof(struct memtype)
		/* and the shared struct itself */
		+ sizeof(struct shmem_shared)
		/* the size of the 2 queues and the queue for the pool */
		+ (insize + outsize) * (2 * sizeof(struct queue_cell))
		/* the size of the pool */
		+ (insize + outsize) * kernel_get_cacheline_size() * CEIL(SAMPLE_LEN(sample_size), kernel_get_cacheline_size())
		/* a memblock for each allocation (1 shmem_shared, 3 queues, 1 pool) */
		+ 5 * sizeof(struct memblock)
		/* and some extra buffer for alignment */
		+ 1024;
}

int shmem_int_open(const char *name, struct shmem_int* shm, struct shmem_conf* conf)
{
	struct shmem_shared *shared;
	pthread_barrierattr_t attr;
	struct memtype *manager;
	struct stat stat;
	size_t len;
	void *base;
	char *cptr;
	int fd, ret;

	shm->name = name;
	fd = shm_open(name, O_RDWR|O_CREAT|O_EXCL, 0600);
	if (fd < 0) {
		if (errno != EEXIST)
			return -1;
		/* Already present; reopen it nonexclusively */
		fd = shm_open(name, O_RDWR, 0);
		if (fd < 0)
			return -1;
		/* Theoretically, the other process might have created the object, but
		 * isn't done with initializing it yet. So in the creating process,
		 * we only reserve a small amount of memory, just enough for the barrier,
		 * and init the barrier, and then resize the object. Thus, here we first
		 * wait for the object to be resized, then wait on the barrier.
		 */
		while (1) {
			if (fstat(fd, &stat) < 0)
				return -1;
			if (stat.st_size > SHMEM_MIN_SIZE)
				break;
		}
		len = stat.st_size;

		base = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (base == MAP_FAILED)
			return -1;

		/* This relies on the behaviour of the node and the allocator; it assumes
		 * that memtype_managed is used and the shmem_shared is the first allocated object */
		cptr = (char *) base + sizeof(struct memtype) + sizeof(struct memblock);
		shared = (struct shmem_shared *) cptr;

		pthread_barrier_wait(&shared->start_bar);
		shm->base = base;
		shm->shared = shared;
		shm->len = 0;
		shm->secondary = 1;

		return 0;
	}
	/* Only map the barrier and init it */
	ret = ftruncate(fd, SHMEM_MIN_SIZE);
	if (ret < 0)
		return -1;
	base = mmap(NULL, SHMEM_MIN_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED)
		return -1;

	/* Again, this assumes that memtype_managed uses first-fit */
	cptr  = (char *) base + sizeof(struct memtype) + sizeof(struct memblock);
	shared = (struct shmem_shared*) cptr;
	pthread_barrierattr_init(&attr);
	pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_barrier_init(&shared->start_bar, &attr, 2);

	/* Remap it with the real size */
	len = shmem_total_size(conf->queuelen, conf->queuelen, conf->samplelen);
	if (munmap(base, SHMEM_MIN_SIZE) < 0)
		return -1;
	if (ftruncate(fd, len) < 0)
		return -1;
	base = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED)
		return -1;

	/* Init everything else */
	manager = memtype_managed_init(base, len);
	shared = memory_alloc(manager, sizeof(struct shmem_shared));
	if (!shared) {
		errno = ENOMEM;
		return -1;
	}

	memset((char *) shared + sizeof(pthread_barrier_t), 0, sizeof(struct shmem_shared) - sizeof(pthread_barrier_t));
	shared->polling = conf->polling;

	ret = shared->polling ? queue_init(&shared->queue[0].q, conf->queuelen, manager)
			   : queue_signalled_init(&shared->queue[0].qs, conf->queuelen, manager);
	if (ret) {
		errno = ENOMEM;
		return -1;
	}

	ret = shared->polling ? queue_init(&shared->queue[1].q, conf->queuelen, manager)
			   : queue_signalled_init(&shared->queue[1].qs, conf->queuelen, manager);
	if (ret) {
		errno = ENOMEM;
		return -1;
	}

	ret = pool_init(&shared->pool, 2 * conf->queuelen, SAMPLE_LEN(conf->samplelen), manager);
	if (ret) {
		errno = ENOMEM;
		return -1;
	}
	shm->base = base;
	shm->len = len;
	shm->shared = shared;
	shm->secondary = 0;
	pthread_barrier_wait(&shared->start_bar);
	return 1;
}

int shmem_int_close(struct shmem_int *shm)
{
	union shmem_queue * queue = &shm->shared->queue[shm->secondary];
	if (shm->shared->polling)
		queue_close(&queue->q);
	else
		queue_signalled_close(&queue->qs);
	if (!shm->secondary)
		/* Ignore the error here; the only thing that is really possible is that
		 * the object was deleted already, which we can't do anything about anyway. */
		shm_unlink(shm->name);

	return munmap(shm->base, shm->len);
}

int shmem_int_read(struct shmem_int *shm, struct sample *smps[], unsigned cnt)
{
	union shmem_queue* queue = &shm->shared->queue[1-shm->secondary];
	return shm->shared->polling ? queue_pull_many(&queue->q, (void **) smps, cnt)
			   : queue_signalled_pull_many(&queue->qs, (void **) smps, cnt);
}

int shmem_int_write(struct shmem_int *shm, struct sample *smps[], unsigned cnt)
{
	union shmem_queue* queue = &shm->shared->queue[shm->secondary];
	return shm->shared->polling ? queue_push_many(&queue->q, (void **) smps, cnt)
			    : queue_signalled_push_many(&queue->qs, (void **) smps, cnt);
}
