/** Heap memory allocator.
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

#include <cstdlib>

#include <villas/utils.hpp>
#include <villas/memory.h>

using namespace villas::utils;

static struct memory_allocation * memory_heap_alloc(size_t len, size_t alignment, struct memory_type *m)
{
	int ret;

	struct memory_allocation *ma = (struct memory_allocation *) alloc(sizeof(struct memory_allocation));
	if (!ma)
		return nullptr;

	ma->alignment = alignment;
	ma->type = m;
	ma->length = len;

	if (ma->alignment < sizeof(void *))
		ma->alignment = sizeof(void *);

	ret = posix_memalign(&ma->address, ma->alignment, ma->length);
	if (ret) {
		free(ma);
		return nullptr;
	}

	return ma;
}

static int memory_heap_free(struct memory_allocation *ma, struct memory_type *m)
{
	free(ma->address);

	return 0;
}

/* List of available memory types */
struct memory_type memory_heap = {
	.name = "heap",
	.flags = (int) MemoryFlags::HEAP,
	.alignment = 1,
	.alloc = memory_heap_alloc,
	.free = memory_heap_free
};
