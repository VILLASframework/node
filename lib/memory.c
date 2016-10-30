/** Memory allocators.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include <stdlib.h>
#include <sys/mman.h>

/* Required to allocate hugepages on Apple OS X */
#ifdef __MACH__
  #include <mach/vm_statistics.h>
#endif

#include "log.h"
#include "memory.h"

void * memory_alloc(const struct memtype *m, size_t len)
{
	debug(DBG_MEM | 2, "Allocating %#zx bytes of %s memory", len, m->name);
	return m->alloc(len);
}

void * memory_alloc_aligned(const struct memtype *m, size_t len, size_t alignment)
{
	debug(DBG_MEM | 2, "Allocating %#zx bytes of %#zx-byte-aligned %s memory", len, alignment, m->name);
	warn("%s: not implemented yet!", __FUNCTION__);
	return memory_alloc(m, len);
}

int memory_free(const struct memtype *m, void *ptr, size_t len)
{
	debug(DBG_MEM | 2, "Releasing %#zx bytes of %s memory", len, m->name);
	return m->free(ptr, len);
}

static void * memory_heap_alloc(size_t len)
{
	return malloc(len);
}

int memory_heap_free(void *ptr, size_t len)
{
	free(ptr);
	
	return 0;
}

/** Allocate memory backed by hugepages with malloc() like interface */
static void * memory_hugepage_alloc(size_t len)
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
