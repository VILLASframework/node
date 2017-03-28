/** Unit tests for memory management
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <criterion/criterion.h>
#include <criterion/theories.h>

#include <errno.h>

#include "memory.h"
#include "utils.h"

TheoryDataPoints(memory, aligned) = {
	DataPoints(size_t, 1, 32, 55, 1 << 10, 1 << 20),
	DataPoints(size_t, 1, 8, 1 << 12),
	DataPoints(struct memtype *, &memtype_heap, &memtype_hugepage)
};

Theory((size_t len, size_t align, struct memtype *m), memory, aligned) {
	int ret;
	void *ptr;
	
	ptr = memory_alloc_aligned(m, len, align);
	cr_assert_neq(ptr, NULL, "Failed to allocate memory");
	
	cr_assert(IS_ALIGNED(ptr, align));

	if (m == &memtype_hugepage) {
		cr_assert(IS_ALIGNED(ptr, HUGEPAGESIZE));
	}

	ret = memory_free(m, ptr, len);
	cr_assert_eq(ret, 0, "Failed to release memory: ret=%d, ptr=%p, len=%zu: %s", ret, ptr, len, strerror(errno));
}

Test(memory, manager) {
    void *p = memory_alloc(&memtype_heap, 1<<10);
    struct memtype *manager = memtype_managed_init(p, 1<<10);

    void *p1, *p2, *p3;
    p1 = memory_alloc(manager, 16);
    cr_assert(p1);

    p2 = memory_alloc(manager, 32);
    cr_assert(p2);

    cr_assert(memory_free(manager, p1, 16) == 0);

    p1 = memory_alloc_aligned(manager, 128, 128);
    cr_assert(p1);
    cr_assert(IS_ALIGNED(p1, 128));

    p3 = memory_alloc_aligned(manager, 128, 256);
    cr_assert(p3);
    cr_assert(IS_ALIGNED(p1, 256));

    cr_assert(memory_free(manager, p2, 32) == 0);
    cr_assert(memory_free(manager, p1, 128) == 0);
    cr_assert(memory_free(manager, p3, 128) == 0);

    p1 = memory_alloc(manager, (1<<10)-sizeof(struct memblock));
    cr_assert(p1);
    cr_assert(memory_free(manager, p1, (1<<10)-sizeof(struct memblock)) == 0);
}
