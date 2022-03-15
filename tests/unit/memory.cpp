/** Unit tests for memory management
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <criterion/criterion.h>
#include <criterion/theories.h>

#include <cerrno>

#include <villas/node/memory.hpp>
#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

using namespace villas;

using namespace villas::node;

extern void init_memory();

#define PAGESIZE (1 << 12)
#define HUGEPAGESIZE (1 << 21)

TheoryDataPoints(memory, aligned) = {
	DataPoints(size_t, 1, 32, 55, 1 << 10, PAGESIZE, HUGEPAGESIZE),
	DataPoints(size_t, 1, 8, PAGESIZE, PAGESIZE),
	DataPoints(struct memory::Type *, &memory::heap, &memory::mmap_hugetlb, &memory::mmap_hugetlb)
};

// cppcheck-suppress unknownMacro
Theory((size_t len, size_t align, struct memory::Type *mt), memory, aligned, .init = init_memory) {
	int ret;
	void *ptr;

	if (!utils::isPrivileged() && mt == &memory::mmap_hugetlb)
		cr_skip_test("Skipping memory_mmap_hugetlb tests allocatpr because we are running in an unprivileged environment.");

	ptr = memory::alloc_aligned(len, align, mt);
	cr_assert_not_null(ptr, "Failed to allocate memory");

	cr_assert(IS_ALIGNED(ptr, align), "Memory at %p is not alligned to %#zx byte bounary", ptr, align);

	if (mt == &memory::mmap_hugetlb) {
		cr_assert(IS_ALIGNED(ptr, HUGEPAGESIZE), "Memory at %p is not alligned to %#x byte bounary", ptr, HUGEPAGESIZE);
	}

	ret = memory::free(ptr);
	cr_assert_eq(ret, 0, "Failed to release memory: ret=%d, ptr=%p, len=%zu: %s", ret, ptr, len, strerror(errno));
}

Test(memory, manager, .init = init_memory) {
	size_t total_size;
	size_t max_block;

	int ret;
	void *p, *p1, *p2, *p3;
	struct memory::Type *m;

	total_size = 1 << 10;
	max_block = total_size - sizeof(struct memory::Type) - sizeof(struct memory::Block);

	p = memory::alloc(total_size, &memory::heap);
	cr_assert_not_null(p);

	m = memory::managed(p, total_size);
	cr_assert_not_null(m);

	p1 = memory::alloc(16, m);
	cr_assert_not_null(p1);

	p2 = memory::alloc(32, m);
	cr_assert_not_null(p2);

	ret = memory::free(p1);
	cr_assert(ret == 0);

	p1 = memory::alloc_aligned(128, 128, m);
	cr_assert_not_null(p1);
	cr_assert(IS_ALIGNED(p1, 128));

	p3 = memory::alloc_aligned(128, 256, m);
	cr_assert(p3);
	cr_assert(IS_ALIGNED(p3, 256));

	ret = memory::free(p2);
	cr_assert(ret == 0);

	ret = memory::free(p1);
	cr_assert(ret == 0);

	ret = memory::free(p3);
	cr_assert(ret == 0);

	p1 = memory::alloc(max_block, m);
	cr_assert_not_null(p1);

	ret = memory::free(p1);
	cr_assert(ret == 0);

	ret = memory::free(p);
	cr_assert(ret == 0);
}
