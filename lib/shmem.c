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
	// we have the constant const of the memtype header
	return sizeof(struct memtype)
		// and the shared struct itself
		+ sizeof(struct shmem_shared)
		// the size of the 2 queues and the queue for the pool
		+ (insize + outsize) * (2*sizeof(struct queue_cell))
		// the size of the pool
		+ (insize + outsize) * kernel_get_cacheline_size() * CEIL(SAMPLE_LEN(sample_size), kernel_get_cacheline_size())
		// a memblock for each allocation (1 shmem_shared, 3 queues, 1 pool)
		+ 5 * sizeof(struct memblock)
		// and some extra buffer for alignment
		+ 1024;
}

struct shmem_shared* shmem_int_open(const char *name, size_t len)
{
	int fd = shm_open(name, O_RDWR, 0);
	if (fd < 0)
		return NULL;
	void *base = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED)
		return NULL;
	/* This relies on the behaviour of the node and the allocator; it assumes
	 * that memtype_managed is used and the shmem_shared is the first allocated object */
	char *cptr = (char *) base + sizeof(struct memtype) + sizeof(struct memblock);
	return (struct shmem_shared *) cptr;
}
