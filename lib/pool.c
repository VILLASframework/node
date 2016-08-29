/** Memory pool for fixed size objects.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include "utils.h"

#include "pool.h"
#include "memory.h"
#include "kernel/kernel.h"

int pool_init(struct pool *p, size_t blocksz, size_t cnt, const struct memtype *m)
{
	void *addr;
	int flags, prot;
	size_t len, alignedsz, alignment;
	
	/* Make sure that we use a block size that is aligned to the size of a cache line */
	alignment = kernel_get_cacheline_size();
	alignedsz = blocksz * CEIL(blocksz, );
	len = cnt * alignedsz;

	addr = memory_alloc_align(m, len, aligment);
	if (!addr)
		serror("Failed to allocate memory for memory pool");
	else
		debug(DBG_POOL | 4, "Allocated %#zx bytes for memory pool", len);
	
	p->blocksz = blocksz;
	p->alignment = alignment;
	
	mpmc_queue_init(&p->queue, cnt, m);
	
	for (int i = 0; i < cnt; i++)
		lstack_push(&p->stack, buf + i * alignedsz);
	
	return 0;
}

int pool_destroy(struct pool *p)
{
	mpmc_queue_destroy(&p->queue);
	
	memory_dealloc(p->buffer, p->len);
}