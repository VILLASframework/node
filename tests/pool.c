/** Unit tests for memory pool
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <signal.h>

#include "pool.h"
#include "utils.h"

struct param {
	int thread_count;
	int pool_size;
	size_t block_size;
	const struct memtype *memtype;
};

ParameterizedTestParameters(pool, basic)
{
	static struct param params[] = {
		{ 1,	4096,	150,	&memtype_heap },
		{ 1,	128,	8,	&memtype_hugepage },
		{ 1,	4,	8192,	&memtype_hugepage },
		{ 1,	1 << 13,4,	&memtype_heap }
	};
	
	return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

ParameterizedTest(struct param *p, pool, basic)
{
	int ret;
	struct pool pool;

	void *ptr, *ptrs[p->pool_size];

	ret = pool_init(&pool, p->pool_size, p->block_size, p->memtype);
	cr_assert_eq(ret, 0, "Failed to create pool");
	
	ptr = pool_get(&pool);
	cr_assert_neq(ptr, NULL);
	
	memset(ptr, 1, p->block_size); /* check that we dont get a seg fault */
	
	for (int i = 1; i < p->pool_size; i++) {
		ptrs[i] = pool_get(&pool);
		cr_assert_neq(ptrs[i], NULL);
	}
	
	ptr = pool_get(&pool);
	cr_assert_eq(ptr, NULL);
	
	ret = pool_destroy(&pool);
	cr_assert_eq(ret, 0, "Failed to destroy pool");

}