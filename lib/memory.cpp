/** Memory allocators.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <unistd.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include <villas/log.hpp>
#include <villas/memory.h>
#include <villas/utils.hpp>
#include <villas/kernel/kernel.hpp>

using namespace villas;

static std::unordered_map<void *, struct memory_allocation *> allocations;
static Logger logger;

int memory_init(int hugepages)
{
	int ret;

	logger = logging.get("memory");

	logger->info("Initialize memory sub-system: #hugepages={}", hugepages);

	ret = memory_mmap_init(hugepages);
	if (ret < 0)
		return ret;

	size_t lock = kernel::getHugePageSize() * hugepages;

	ret = memory_lock(lock);
	if (ret)
		return ret;

	return 0;
}

int memory_lock(size_t lock)
{
	int ret;

	if (!utils::isPrivileged()) {
		logger->warn("Running in an unprivileged environment. Memory is not locked to RAM!");
		return 0;
	}

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
				logger->warn("Failed to increase ressource limit of locked memory. Please increase manually by running as root:");
				logger->warn("   $ ulimit -Hl {}", lock);

				return 0;
			}

			l.rlim_max = lock;
		}

		l.rlim_cur = lock;

		ret = setrlimit(RLIMIT_MEMLOCK, &l);
		if (ret)
			return ret;

		logger->debug("Increased ressource limit of locked memory to {} bytes", lock);
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

void * memory_alloc(size_t len, struct memory_type *m)
{
	return memory_alloc_aligned(len, sizeof(void *), m);
}

void * memory_alloc_aligned(size_t len, size_t alignment, struct memory_type *m)
{
	struct memory_allocation *ma = m->alloc(len, alignment, m);
	if (ma == nullptr) {
		logger->warn("Memory allocation of type {} failed. reason={}", m->name, strerror(errno));
		return nullptr;
	}

	allocations[ma->address] = ma;

	logger->debug("Allocated {:#x} bytes of {:#x}-byte-aligned {} memory: {}", ma->length, ma->alignment, ma->type->name, ma->address);

	return ma->address;
}

int memory_free(void *ptr)
{
	int ret;

	/* Find corresponding memory allocation entry */
	struct memory_allocation *ma = allocations[ptr];
	if (!ma)
		return -1;

	logger->debug("Releasing {:#x} bytes of {} memory: {}", ma->length, ma->type->name, ma->address);

	ret = ma->type->free(ma, ma->type);
	if (ret)
		return ret;

	/* Remove allocation entry */
	auto iter = allocations.find(ptr);
	if (iter == allocations.end())
		return -1;

	allocations.erase(iter);
	delete ma;

	return 0;
}

struct memory_allocation * memory_get_allocation(void *ptr)
{
	return allocations[ptr];
}

struct memory_type *memory_default = nullptr;
