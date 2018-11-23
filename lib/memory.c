/** Memory allocators.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

__attribute__((constructor))
static void init_allocations()
{
	hash_table_init(&allocations, 100);
}

__attribute__((destructor))
static void destroy_allocations()
{
	/** @todo: Release remaining allocations? */
	hash_table_destroy(&allocations, NULL, false);
}

int memory_init(int hugepages)
{
	int ret;

	info("Initialize memory sub-system: #hugepages=%d", hugepages);

	/* Initialize hugepage allocator */
	ret = memory_hugepage_init();
	if (ret)
		return ret;

#if defined(__linux__) && defined(__x86_64__)
	int pagecnt, pagesz, ret;
	struct rlimit l;

	pagecnt = kernel_get_nr_hugepages();
	if (pagecnt < hugepages) {
		if (getuid() == 0) {
			kernel_set_nr_hugepages(hugepages);
			debug(LOG_MEM | 2, "Increased number of reserved hugepages from %d to %d", pagecnt, hugepages);
		}
		else {
			warn("Failed to reserved hugepages. Please re-run as super-user or reserve manually via:");
			warn("   $ echo %d > /proc/sys/vm/nr_hugepages", hugepages);

			return -1;
		}
	}

	pagesz = kernel_get_hugepage_size();
	if (pagesz < 0)
		return -1;

	/* Amount of memory which should be lockable */
	size_t lock = pagesz * hugepages;

	ret = getrlimit(RLIMIT_MEMLOCK, &l);
	if (ret)
		return ret;

	if (l.rlim_cur < lock) {
		if (l.rlim_max < lock) {
			if (getuid() != 0) {
				warn("Failed to in increase ressource limit of locked memory from %lu to %zu bytes", l.rlim_cur, lock);
				warn("Please re-run as super-user or raise manually via:");
				warn("   $ ulimit -Hl %zu", lock);

				return -1;
			}

			l.rlim_max = lock;
		}

		l.rlim_cur = lock;

		ret = setrlimit(RLIMIT_MEMLOCK, &l);
		if (ret)
			return ret;

		debug(LOG_MEM | 2, "Increased ressource limit of locked memory to %d bytes", lock);
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
	int ret;

	struct memory_allocation *ma = m->alloc(m, len, alignment);
	if (ma == NULL) {
		warn("memory_alloc_aligned: allocating memory for memory_allocation failed for memory type %s. Reason: %s", m->name, strerror(errno) );
		return NULL;
	}

	ret = hash_table_insert(&allocations, ma->address, ma);
	if (ret) {
		warn("memory_alloc_aligned: Inserting into hash table failed!");
		return NULL;
	}

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

	debug(LOG_MEM | 5, "Releasing %#zx bytes of %s memory: %p", ma->length, ma->type->name, ma->address);

	ret = ma->type->free(ma->type, ma);
	if (ret)
		return ret;

	/* Remove allocation entry */
	ret = hash_table_delete(&allocations, ptr);
	if (ret)
		return ret;

	free(ma);

	return 0;
}

struct memory_allocation * memory_get_allocation(void *ptr)
{
	struct memory_allocation *ma = (struct memory_allocation *) hash_table_lookup(&allocations, ptr);
	return ma;
}

struct memory_type * memory_type_lookup(enum memory_type_flags flags)
{
	if (flags & MEMORY_HUGEPAGE)
		return &memory_hugepage;
	else if (flags & MEMORY_HEAP)
		return &memory_heap;
	else
		return NULL;
}
