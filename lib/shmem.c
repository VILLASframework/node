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
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "kernel/kernel.h"
#include "memory.h"
#include "utils.h"
#include "sample.h"
#include "shmem.h"

size_t shmem_total_size(int queuelen, int samplelen)
{
	/* We have the constant const of the memtype header */
	return sizeof(struct memtype)
		/* and the shared struct itself */
		+ sizeof(struct shmem_shared)
		/* the size of the actual queue and the queue for the pool */
		+ queuelen * (2 * sizeof(struct queue_cell))
		/* the size of the pool */
		+ queuelen * kernel_get_cacheline_size() * CEIL(SAMPLE_LEN(samplelen), kernel_get_cacheline_size())
		/* a memblock for each allocation (1 shmem_shared, 2 queues, 1 pool) */
		+ 4 * sizeof(struct memblock)
		/* and some extra buffer for alignment */
		+ 1024;
}

int shmem_int_open(const char *wname, const char* rname, struct shmem_int *shm, struct shmem_conf *conf)
{
	char *cptr;
	int fd, ret;
	size_t len;
	void *base;
	struct memtype *manager;
	struct shmem_shared *shared;
	struct stat stat_buf;
	sem_t *sem_own, *sem_other;

	/* Ensure both semaphores exist */
	sem_own = sem_open(wname, O_CREAT, 0600, 0);
	if (sem_own == SEM_FAILED)
		return -1;

	sem_other = sem_open(rname, O_CREAT, 0600, 0);
	if (sem_other == SEM_FAILED)
		return -1;

	/* Open and initialize the shared region for the output queue */
	fd = shm_open(wname, O_RDWR|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		return -1;

	len = shmem_total_size(conf->queuelen, conf->samplelen);
	if (ftruncate(fd, len) < 0)
		return -1;

	base = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED)
		return -1;

	close(fd);

	manager = memtype_managed_init(base, len);
	shared = memory_alloc(manager, sizeof(struct shmem_shared));
	if (!shared) {
		errno = ENOMEM;
		return -1;
	}

	memset(shared, 0, sizeof(struct shmem_shared));
	shared->polling = conf->polling;
	
	int flags = QUEUE_SIGNALLED_PROCESS_SHARED;
	if (conf->polling)
		flags |= QUEUE_SIGNALLED_POLLING;
	else
		flags |= QUEUE_SIGNALLED_PTHREAD;

	ret = queue_signalled_init(&shared->queue, conf->queuelen, manager, flags);
	if (ret) {
		errno = ENOMEM;
		return -1;
	}

	ret = pool_init(&shared->pool, conf->queuelen, SAMPLE_LEN(conf->samplelen), manager);
	if (ret) {
		errno = ENOMEM;
		return -1;
	}

	shm->write.base = base;
	shm->write.name = wname;
	shm->write.len = len;
	shm->write.shared = shared;

	/* Post own semaphore and wait on the other one, so both processes know that
	 * both regions are initialized */
	sem_post(sem_own);
	sem_wait(sem_other);

	/* Open and map the other region */
	fd = shm_open(rname, O_RDWR, 0);
	if (fd < 0)
		return -1;

	if (fstat(fd, &stat_buf) < 0)
		return -1;

	len = stat_buf.st_size;
	base = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (base == MAP_FAILED)
		return -1;

	cptr = (char *) base + sizeof(struct memtype) + sizeof(struct memblock);
	shared = (struct shmem_shared *) cptr;
	shm->read.base = base;
	shm->read.name = rname;
	shm->read.len = len;
	shm->read.shared = shared;

	shm->readers = 0;
	shm->writers = 0;
	shm->closed = 0;

	/* Unlink the semaphores; we don't need them anymore */
	sem_unlink(wname);

	return 0;
}

int shmem_int_close(struct shmem_int *shm)
{
	atomic_store(&shm->closed, 1);
	
	queue_signalled_close(&shm->write.shared->queue);

	shm_unlink(shm->write.name);
	if (atomic_load(&shm->readers) == 0)
		munmap(shm->read.base, shm->read.len);
	if (atomic_load(&shm->writers) == 0)
		munmap(shm->write.base, shm->write.len);

	return 0;
}

int shmem_int_read(struct shmem_int *shm, struct sample *smps[], unsigned cnt)
{
	int ret;

	atomic_fetch_add(&shm->readers, 1);

	ret = queue_signalled_pull_many(&shm->read.shared->queue, (void **) smps, cnt);

	if (atomic_fetch_sub(&shm->readers, 1) == 1 && atomic_load(&shm->closed) == 1)
		munmap(shm->read.base, shm->read.len);

	return ret;
}

int shmem_int_write(struct shmem_int *shm, struct sample *smps[], unsigned cnt)
{
	int ret;

	atomic_fetch_add(&shm->writers, 1);

	ret = queue_signalled_push_many(&shm->write.shared->queue, (void **) smps, cnt);

	if (atomic_fetch_sub(&shm->writers, 1) == 1 && atomic_load(&shm->closed) == 1)
		munmap(shm->write.base, shm->write.len);

	return ret;
}

int shmem_int_alloc(struct shmem_int *shm, struct sample *smps[], unsigned cnt)
{
	return sample_alloc_many(&shm->write.shared->pool, smps, cnt);
}
