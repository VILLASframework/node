/** Memory allocators.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdlib.h>
#include <sys/mman.h>

/* Required to allocate hugepages on Apple OS X */
#ifdef __MACH__
  #include <mach/vm_statistics.h>
#elif defined(__linux__)
  #include "kernel/kernel.h"
#endif

#include "log.h"
#include "memory.h"

int memory_init()
{ INDENT
#ifdef __linux__
	int nr = kernel_get_nr_hugepages();
	
	if (nr < DEFAULT_NR_HUGEPAGES) {
		kernel_set_nr_hugepages(DEFAULT_NR_HUGEPAGES);
		debug(LOG_MEM | 2, "Reserve %d hugepages (was %d)", DEFAULT_NR_HUGEPAGES, nr);
	}
#endif
	return 0;
}

void * memory_alloc(const struct memtype *m, size_t len)
{
	void *ptr = m->alloc(len, sizeof(void *));
		
	debug(LOG_MEM | 2, "Allocated %#zx bytes of %s memory: %p", len, m->name, ptr);

	return ptr;
}

void * memory_alloc_aligned(const struct memtype *m, size_t len, size_t alignment)
{
	void *ptr = m->alloc(len, alignment);
	
	debug(LOG_MEM | 2, "Allocated %#zx bytes of %#zx-byte-aligned %s memory: %p", len, alignment, m->name, ptr);
	
	return ptr;
}

int memory_free(const struct memtype *m, void *ptr, size_t len)
{
	debug(LOG_MEM | 2, "Releasing %#zx bytes of %s memory", len, m->name);
	return m->free(ptr, len);
}

static void * memory_heap_alloc(size_t len, size_t alignment)
{
	void *ptr;
	int ret;
	
	if (alignment < sizeof(void *))
		alignment = sizeof(void *);
	
	ret = posix_memalign(&ptr, alignment, len);
	
	return ret ? NULL : ptr;
}

int memory_heap_free(void *ptr, size_t len)
{
	free(ptr);
	
	return 0;
}

/** Allocate memory backed by hugepages with malloc() like interface */
static void * memory_hugepage_alloc(size_t len, size_t alignment)
{
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	
#ifdef __MACH__
	flags |= VM_FLAGS_SUPERPAGE_SIZE_2MB;
#elif defined(__linux__)
	flags |= MAP_HUGETLB | MAP_LOCKED;
#endif
	
	void *ret = mmap(NULL, len, prot, flags, -1, 0);
	
	if (ret == MAP_FAILED) {
		info("Failed to allocate huge pages: Check https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt");
		return NULL;
	}
	
	return ret;
}

static int memory_hugepage_free(void *ptr, size_t len)
{
	len = ALIGN(len, HUGEPAGESIZE); /* ugly see: https://lkml.org/lkml/2015/3/27/171 */

	return munmap(ptr, len);
}

/* List of available memory types */
const struct memtype memtype_heap = {
	.name = "heap",
	.flags = MEMORY_HEAP,
	.alloc = memory_heap_alloc,
	.free = memory_heap_free,
	.alignment = 1
};

const struct memtype memtype_hugepage = {
	.name = "mmap_hugepages",
	.flags = MEMORY_MMAP | MEMORY_HUGEPAGE,
	.alloc = memory_hugepage_alloc,
	.free = memory_hugepage_free,
	.alignment = 21  /* 2 MiB hugepage */
};

/** @todo */
const struct memtype memtype_dma = {
	.name = "dma",
	.flags = MEMORY_DMA | MEMORY_MMAP,
	.alloc = NULL, .free = NULL,
	.alignment = 12
};
