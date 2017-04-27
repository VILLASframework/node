/** Unit tests for memory pool
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <signal.h>

#include "pool.h"
#include "utils.h"

struct param {
	int thread_count;
	int pool_size;
	size_t block_size;
	struct memtype *memtype;
};

ParameterizedTestParameters(pool, basic)
{
	static struct param params[] = {
		{ 1,	4096,	150,	&memtype_heap },
		{ 1,	128,	8,	&memtype_hugepage },
		{ 1,	4,	8192,	&memtype_hugepage },
		{ 1,	1 << 13, 4,	&memtype_heap }
	};
	
	return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

ParameterizedTest(struct param *p, pool, basic)
{
	int ret;
	struct pool pool = { .state = STATE_DESTROYED };

	void *ptr, *ptrs[p->pool_size];

	ret = pool_init(&pool, p->pool_size, p->block_size, p->memtype);
	cr_assert_eq(ret, 0, "Failed to create pool");
	
	ptr = pool_get(&pool);
	cr_assert_neq(ptr, NULL);
	
	memset(ptr, 1, p->block_size); /* check that we dont get a seg fault */
	
	int i;
	for (i = 1; i < p->pool_size; i++) {
		ptrs[i] = pool_get(&pool);

		if (ptrs[i] == NULL)
			break;
	}
	
	if (i < p->pool_size)
		cr_assert_neq(ptrs[i], NULL);
	
	ptr = pool_get(&pool);
	cr_assert_eq(ptr, NULL);
	
	ret = pool_destroy(&pool);
	cr_assert_eq(ret, 0, "Failed to destroy pool");

}
