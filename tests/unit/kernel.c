/** Unit tests for kernel functions.
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

#include <villas/kernel/kernel.h>
#include <villas/utils.h>

#ifdef __linux__

#if defined(__x86_64__) || defined(__i386__)
  #define PAGESIZE (1 << 12)
  #define CACHELINESIZE 64

  #if defined(__x86_64__)
    #define HUGEPAGESIZE (1 << 21)
  #elif defined(__i386__)
    #define HUGEPAGESIZE (1 << 22)
  #endif
#else
  #error "Unsupported architecture"
#endif

/* This test is not portable, but we currently support x86 only */
Test(kernel, sizes)
{
	int sz;

	sz = kernel_get_page_size();
	cr_assert_eq(sz, PAGESIZE);

	sz = kernel_get_hugepage_size();
	cr_assert(sz == HUGEPAGESIZE);

	sz = kernel_get_cacheline_size();
	cr_assert_eq(sz, CACHELINESIZE);
}

Test(kernel, hugepages)
{
	int ret;

	ret = kernel_set_nr_hugepages(25);
	cr_assert_eq(ret, 0);

	ret = kernel_get_nr_hugepages();
	cr_assert_eq(ret, 25);

	ret = kernel_set_nr_hugepages(10);
	cr_assert_eq(ret, 0);

	ret = kernel_get_nr_hugepages();
	cr_assert_eq(ret, 10);
}

Test(kernel, version)
{
	int ret;

	struct version ver;

	ret = kernel_get_version(&ver);
	cr_assert_eq(ret, 0);

	ret = version_cmp(&ver, &(struct version) { 100, 5 });
	cr_assert_lt(ret, 0);

	ret = version_cmp(&ver, &(struct version) { 2, 6 });
	cr_assert_gt(ret, 0);
}

Test(kernel, module, .disabled = true)
{
	int ret;

	ret = kernel_module_loaded("nf_nat");
	cr_assert_eq(ret, 0);

	ret = kernel_module_loaded("does_not_exist");
	cr_assert_neq(ret, 0);
}

Test(kernel, frequency)
{
	int ret;
	uint64_t freq;

	ret = kernel_get_cpu_frequency(&freq);
	cr_assert_eq(ret, 0);

	/* Check for plausability only */
	cr_assert(freq > 1e9 && freq < 5e9);
}

#endif /* __linux__ */
