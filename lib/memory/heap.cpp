/** Heap memory allocator.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/node/memory.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::memory;

static
struct Allocation * heap_alloc(size_t len, size_t alignment, struct Type *m)
{
	int ret;

	auto *ma = new struct Allocation;
	if (!ma)
		throw MemoryAllocationError();

	ma->alignment = alignment;
	ma->type = m;
	ma->length = len;

	if (ma->alignment < sizeof(void *))
		ma->alignment = sizeof(void *);

	ret = posix_memalign(&ma->address, ma->alignment, ma->length);
	if (ret) {
		delete ma;
		return nullptr;
	}

	return ma;
}

static
int heap_free(struct Allocation *ma, struct Type *m)
{
	::free(ma->address);

	return 0;
}

/* List of available memory types */
struct Type villas::node::memory::heap = {
	.name = "heap",
	.flags = (int) Flags::HEAP,
	.alignment = 1,
	.alloc = heap_alloc,
	.free = heap_free
};
