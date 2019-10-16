/** Memory allocators.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <unordered_map>

#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <strings.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include <villas/log.h>
#include <villas/memory.h>
#include <villas/utils.hpp>
#include <villas/kernel/kernel.h>

static std::unordered_map<void *, struct memory_allocation *> allocations;

int memory_init(int hugepages)
{
	int ret;

	info("Initialize memory sub-system: #hugepages=%d", hugepages);

	ret = memory_hugepage_init(hugepages);
	if (ret)
		return ret;

	size_t lock = kernel_get_hugepage_size() * hugepages;

	ret = memory_lock(lock);
	if (ret)
		return ret;

	return 0;
}

int memory_lock(size_t lock)
{
	int ret;

#ifdef __linux__

#ifndef __arm__
	struct rlimit l;

	/* Increase ressource limit for locked memory */
	ret = getrlimit(RLIMIT_MEMLOCK, &l);
	if (ret)
		return ret;

	if (l.rlim_cur < lock) {
		if (l.rlim_max < lock) {
			if (getuid() != 0) {
				warning("Failed to in increase ressource limit of locked memory. Please increase manually by running as root:");
				warning("   $ ulimit -Hl %zu", lock);

				return 0;
			}

			l.rlim_max = lock;
		}

		l.rlim_cur = lock;

		ret = setrlimit(RLIMIT_MEMLOCK, &l);
		if (ret)
			return ret;

		debug(LOG_MEM | 2, "Increased ressource limit of locked memory to %zd bytes", lock);
	}

#endif /* __arm__ */
#ifdef _POSIX_MEMLOCK
	/* Lock all current and future memory allocations */
	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret)
		return -1;
#endif /* _POSIX_MEMLOCK */

#endif /* __linux__ */

	return 0;
}

void * memory_alloc(struct memory_type *m, size_t len)
{
	return memory_alloc_aligned(m, len, sizeof(void *));
}

void * memory_alloc_aligned(struct memory_type *m, size_t len, size_t alignment)
{
	struct memory_allocation *ma = m->alloc(m, len, alignment);
	if (ma == nullptr) {
		warning("Memory allocation of type %s failed. reason=%s", m->name, strerror(errno) );
		return nullptr;
	}

	allocations[ma->address] = ma;

	debug(LOG_MEM | 5, "Allocated %#zx bytes of %#zx-byte-aligned %s memory: %p", ma->length, ma->alignment, ma->type->name, ma->address);

	return ma->address;
}

int memory_free(void *ptr)
{
	int ret;

	/* Find corresponding memory allocation entry */
	struct memory_allocation *ma = allocations[ptr];
	if (!ma)
		return -1;

	debug(LOG_MEM | 5, "Releasing %#zx bytes of %s memory: %p", ma->length, ma->type->name, ma->address);

	ret = ma->type->free(ma->type, ma);
	if (ret)
		return ret;

	/* Remove allocation entry */
	auto iter = allocations.find(ptr);
  	if (iter == allocations.end())
		return -1;

    allocations.erase(iter);
	free(ma);

	return 0;
}

struct memory_allocation * memory_get_allocation(void *ptr)
{
	return allocations[ptr];
}

struct memory_type * memory_type_lookup(enum MemoryFlags flags)
{
	if ((int) flags & (int) MemoryFlags::HUGEPAGE)
		return &memory_hugepage;
	else if ((int) flags & (int) MemoryFlags::HEAP)
		return &memory_heap;
	else
		return nullptr;
}
