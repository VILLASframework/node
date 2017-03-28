/** Memory allocators.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 */

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
{
#ifdef __linux__
	int nr = kernel_get_nr_hugepages();
	
	debug(DBG_MEM | 2, "System has %d reserved hugepages", nr);
	
	if (nr < DEFAULT_NR_HUGEPAGES)
		kernel_set_nr_hugepages(DEFAULT_NR_HUGEPAGES);
#endif
	return 0;
}

void * memory_alloc(struct memtype *m, size_t len)
{
	void *ptr = m->alloc(m, len, sizeof(void *));
		
	debug(DBG_MEM | 2, "Allocated %#zx bytes of %s memory: %p", len, m->name, ptr);

	return ptr;
}

void * memory_alloc_aligned(struct memtype *m, size_t len, size_t alignment)
{
	void *ptr = m->alloc(m, len, alignment);
	
	debug(DBG_MEM | 2, "Allocated %#zx bytes of %#zx-byte-aligned %s memory: %p", len, alignment, m->name, ptr);
	
	return ptr;
}

int memory_free(struct memtype *m, void *ptr, size_t len)
{
	debug(DBG_MEM | 2, "Releasing %#zx bytes of %s memory", len, m->name);
	return m->free(m, ptr, len);
}

static void * memory_heap_alloc(struct memtype *m, size_t len, size_t alignment)
{
	void *ptr;
	int ret;
	
	if (alignment < sizeof(void *))
		alignment = sizeof(void *);
	
	ret = posix_memalign(&ptr, alignment, len);
	
	return ret ? NULL : ptr;
}

int memory_heap_free(struct memtype *m, void *ptr, size_t len)
{
	free(ptr);
	
	return 0;
}

/** Allocate memory backed by hugepages with malloc() like interface */
static void * memory_hugepage_alloc(struct memtype *m, size_t len, size_t alignment)
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

static int memory_hugepage_free(struct memtype *m, void *ptr, size_t len)
{
	len = ALIGN(len, HUGEPAGESIZE); /* ugly see: https://lkml.org/lkml/2015/3/27/171 */

	return munmap(ptr, len);
}

void* memory_managed_alloc(struct memtype *m, size_t len, size_t alignment)
{
    // Simple first-fit allocation.
    struct memtype_managed* man = (struct memtype_managed*) m;
    struct memblock* block;
    for (block = man->first; block != NULL; block = block->next) {
        if (block->flags & MEMBLOCK_USED)
            continue;
        char* cptr = (char*) block + sizeof(struct memblock);
        size_t avail = block->len;
        uintptr_t uptr = (uintptr_t) cptr;
        // check alignment first; leave a gap at start of block to assure
        // alignment if necessary
        uintptr_t rem = uptr % alignment;
        uintptr_t gap = 0;
        if (rem != 0) {
            gap = alignment - rem;
            cptr += gap;
            avail -= gap;
        }
        if (avail >= len) {
            if (gap > sizeof(struct memblock)) {
                // the alignment gap is big enough to fit another block.
                // the original block descriptor is already at the correct
                // position, so we just change its len and create a new block
                // descriptor for the actual block we're handling.
                block->len = gap;
                struct memblock *newblock = (struct memblock*) (cptr - sizeof(struct memblock));
                newblock->prev = block;
                newblock->next = block->next;
                block->next = newblock;
                newblock->flags = 0;
                newblock->len = len;
                block = newblock;
            } else {
                // the gap is too small to fit another block descriptor, so we
                // must account for the gap length in the block length.
                block->len = len + gap;
            }
            if (avail > len + sizeof(struct memblock)) {
                // imperfect fit, so create another block for the remaining part
                struct memblock *newblock = (struct memblock*) (cptr + len);
                newblock->prev = block;
                newblock->next = block->next;
                block->next = newblock;
                if (newblock->next)
                    newblock->next->prev = newblock;
                newblock->flags = 0;
                newblock->len = avail - len - sizeof(struct memblock);
            } else {
                // if this block was larger than the requested length, but only
                // by less than sizeof(struct memblock), we may have wasted
                // memory by previous assignments to block->len
                block->len = avail;
            }
            block->flags |= MEMBLOCK_USED;
            return (void*) cptr;
        }
    }
    // no suitable block found
    return NULL;
}

int memory_managed_free(struct memtype *m, void *ptr, size_t len)
{
    struct memtype_managed* man = (struct memtype_managed*) m;
    char* cptr = (char*) ptr;
    struct memblock* block;
    for (block = man->first; block != NULL; block = block->next) {
        if (!(block->flags & MEMBLOCK_USED))
            continue;
        // since we may waste some memory at the start of a block to ensure
        // alignment, ptr may not actually be the start of the block
        if ((char*) block + sizeof(struct memblock) <= cptr &&
            cptr < (char*) block + sizeof(struct memblock) + block->len) {
            // try to merge it with a neighbouring free block
            if (block->prev && !(block->prev->flags & MEMBLOCK_USED)) {
                block->prev->len += block->len + sizeof(struct memblock);
                block->prev->next = block->next;
                if (block->next)
                    block->next->prev = block->prev;
            } else if (block->next && !(block->next->flags & MEMBLOCK_USED)) {
                block->len += block->next->len + sizeof(struct memblock);
                block->next = block->next->next;
                if (block->next)
                    block->next->prev = block;
            } else {
                // no neighbouring free block, so just mark it as free
                block->flags &= (~MEMBLOCK_USED);
            }
            return 0;
        }
    }
    return -1;
}

struct memtype* memtype_managed_init(void *ptr, size_t len)
{
    if (len < sizeof(struct memtype_managed) + sizeof(struct memblock)) {
        info("memtype_managed: passed region too small");
        return NULL;
    }
    struct memtype_managed *man = (struct memtype_managed*) ptr;
    man->mt.name = "managed";
    man->mt.flags = 0; // TODO
    man->mt.alloc = memory_managed_alloc;
    man->mt.free = memory_managed_free;
    man->mt.alignment = 1;
    man->base = ptr;
    man->len = len;

    char *cptr = (char*) ptr;
    cptr += ALIGN(sizeof(struct memtype_managed), sizeof(void*));
    man->first = (struct memblock*) ((void*) cptr);
    man->first->prev = NULL;
    man->first->next = NULL;
    cptr += ALIGN(sizeof(struct memblock), sizeof(void*));
    man->first->len = len - (cptr - (char*) ptr);
    man->first->flags = 0;

    return (struct memtype*) man;
}

/* List of available memory types */
struct memtype memtype_heap = {
	.name = "heap",
	.flags = MEMORY_HEAP,
	.alloc = memory_heap_alloc,
	.free = memory_heap_free,
	.alignment = 1
};

struct memtype memtype_hugepage = {
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
