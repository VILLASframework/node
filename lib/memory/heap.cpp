/* Heap memory allocator.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

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

// List of available memory types
struct Type villas::node::memory::heap = {
	.name = "heap",
	.flags = (int) Flags::HEAP,
	.alignment = 1,
	.alloc = heap_alloc,
	.free = heap_free
};
