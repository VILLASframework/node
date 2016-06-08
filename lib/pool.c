/** Memory pool for fixed size objects.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include <sys/mman.h>

#include "utils.h"

#include "pool.h"
#include "kernel.h"

int pool_init_mmap(struct pool *p, size_t blocksz, size_t cnt)
{
	void *addr;
	int flags;
	size_t len, alignedsz, align;
	
	align = kernel_get_cacheline_size();
	alignedsz = blocksz * CEIL(blocksz, align);
	len = cnt * alignedsz;
	
	debug(DBG_POOL | 4, "Allocating %#zx bytes for memory pool", len);
	
	flags = MAP_LOCKED | MAP_PRIVATE | MAP_ANONYMOUS; // MAP_HUGETLB
	/** @todo Use hugepages */

	/* addr is allways aligned to pagesize boundary */
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (addr == MAP_FAILED)
		serror("Failed to allocate memory for sample pool");
 
	return pool_init(p, blocksz, align, addr, len);
}

int pool_init(struct pool *p, size_t blocksz, size_t align, void *buf, size_t len)
{
	size_t alignedsz, cnt;
	
	assert(IS_ALIGNED(buf, align)); /* buf has to be aligned */
	
	p->blocksz = blocksz;
	p->alignment = align;
	
	alignedsz = blocksz * CEIL(blocksz, align);
	cnt = len / alignedsz;
	
	lstack_init(&p->stack, cnt);
	
	for (int i = 0; i < cnt; i++)
		lstack_push(&p->stack, buf + i * alignedsz);
	
	return 0;
}