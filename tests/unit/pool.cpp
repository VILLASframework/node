/* Unit tests for memory pool.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <vector>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <signal.h>

#include <villas/log.hpp>
#include <villas/pool.hpp>
#include <villas/utils.hpp>

using namespace villas;

using namespace villas::node;

extern void init_memory();

struct param {
  int thread_count;
  int pool_size;
  size_t block_size;
  // Criterion builds the parameters and runs the test in different processes.
  // In a PIE binary, addresses of static objects like memory::heap differ.
  enum class MemoryType { HEAP, MMAP, MMAP_HUGETLB } mt;
};

static struct memory::Type *memory_type(param::MemoryType mt) {
  switch (mt) {
  case param::MemoryType::HEAP:
    return &memory::heap;
  case param::MemoryType::MMAP:
    return &memory::mmap;
  case param::MemoryType::MMAP_HUGETLB:
    return &memory::mmap_hugetlb;
  }

  return nullptr;
}

ParameterizedTestParameters(pool, basic) {
  static std::vector<struct param> params;

  params.clear();
  params.push_back({1, 4096, 150, param::MemoryType::HEAP});
  params.push_back({1, 128, 8, param::MemoryType::MMAP});
  params.push_back({1, 4, 8192, param::MemoryType::MMAP_HUGETLB});
  params.push_back({1, 1 << 13, 4, param::MemoryType::MMAP_HUGETLB});

  return cr_make_param_array(struct param, params.data(), params.size());
}

// cppcheck-suppress unknownMacro
ParameterizedTest(struct param *p, pool, basic, .init = init_memory) {
  int ret;
  struct Pool pool;
  void *ptr, *ptrs[p->pool_size];

  Log::getInstance().setLevel("trace");

  if (!utils::isPrivileged() && p->mt == param::MemoryType::MMAP_HUGETLB)
    cr_skip_test("Skipping memory_mmap_hugetlb tests allocatpr because we are "
                 "running in an unprivileged environment.");

  ret = pool_init(&pool, p->pool_size, p->block_size, memory_type(p->mt));
  cr_assert_eq(ret, 0, "Failed to create pool");

  ptr = pool_get(&pool);
  cr_assert_neq(ptr, nullptr);

  memset(ptr, 1, p->block_size); // check that we dont get a seg fault

  int i;
  for (i = 1; i < p->pool_size; i++) {
    ptrs[i] = pool_get(&pool);

    if (ptrs[i] == nullptr)
      break;
  }

  if (i < p->pool_size)
    cr_assert_neq(ptrs[i], nullptr);

  ptr = pool_get(&pool);
  cr_assert_eq(ptr, nullptr);

  ret = pool_destroy(&pool);
  cr_assert_eq(ret, 0, "Failed to destroy pool");
}
