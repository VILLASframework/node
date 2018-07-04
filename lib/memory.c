/** Memory allocators.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <villas/log.h>
#include <villas/memory.h>
#include <villas/utils.h>
#include <villas/hash_table.h>
#include <villas/kernel/kernel.h>

static struct hash_table allocations = { .state = STATE_DESTROYED };

int memory_init(int hugepages)
{
	int ret;

	if (allocations.state == STATE_DESTROYED) {
		ret = hash_table_init(&allocations, 100);
		if (ret)
			return ret;
	}

#ifdef __linux__
	int pagecnt, pagesz;
	struct rlimit l;

	info("Initialize memory sub-system");

	pagecnt = kernel_get_nr_hugepages();
	if (pagecnt < hugepages) { INDENT
		kernel_set_nr_hugepages(hugepages);
		debug(LOG_MEM | 2, "Reserved %d hugepages (was %d)", hugepages, pagecnt);
	}

	pagesz = kernel_get_hugepage_size();
	if (pagesz < 0)
		return -1;

	ret = getrlimit(RLIMIT_MEMLOCK, &l);
	if (ret)
		return ret;

	if (l.rlim_cur < pagesz * pagecnt) { INDENT
		l.rlim_cur = pagesz * pagecnt;
		l.rlim_max = l.rlim_cur;

		ret = setrlimit(RLIMIT_MEMLOCK, &l);
		if (ret)
			return ret;

		debug(LOG_MEM | 2, "Increased ressource limit of locked memory to %d bytes", pagesz * pagecnt);
	}
#endif
	return 0;
}

void * memory_alloc(struct memory_type *m, size_t len)
{
	return memory_alloc_aligned(m, len, sizeof(void *));
}

void * memory_alloc_aligned(struct memory_type *m, size_t len, size_t alignment)
{
	struct memory_allocation *ma = m->alloc(m, len, alignment);

	hash_table_insert(&allocations, ma->address, ma);

	debug(LOG_MEM | 5, "Allocated %#zx bytes of %#zx-byte-aligned %s memory: %p", ma->length, ma->alignment, ma->type->name, ma->address);

	return ma->address;
}

int memory_free(void *ptr)
{
	int ret;

	/* Find corresponding memory allocation entry */
	struct memory_allocation *ma = (struct memory_allocation *) hash_table_lookup(&allocations, ptr);
	if (!ma)
		return -1;

	debug(LOG_MEM | 5, "Releasing %#zx bytes of %s memory", ma->length, ma->type->name);

	ret = ma->type->free(ma->type, ma);
	if (ret)
		return ret;

	/* Remove allocation entry */
	ret = hash_table_delete(&allocations, ma->address);
	if (ret)
		return ret;

	return 0;
}
