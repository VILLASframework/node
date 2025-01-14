/* Unit tests for config features.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>
#include <cstdio>
#include <cstdlib>

#include <villas/config_class.hpp>
#include <villas/utils.hpp>

using namespace villas::node;

// cppcheck-suppress syntaxError
Test(config, env) {
  const char *cfg_f = "test = \"${MY_ENV_VAR}\"\n";

  std::FILE *f = std::tmpfile();
  std::fputs(cfg_f, f);
  std::rewind(f);

  auto c = Config();

  char env[] = "MY_ENV_VAR=mobydick";
  putenv(env);

  auto *r = c.load(f);
  cr_assert_not_null(r);

  auto *j = json_object_get(r, "test");
  cr_assert_not_null(j);

  cr_assert(json_is_string(j));
  cr_assert_str_eq("mobydick", json_string_value(j));
}

Test(config, include) {
  const char *cfg_f2 = "magic = 1234\n";

  char f2_fn_tpl[] = "/tmp/villas.unit-test.XXXXXX";
  int f2_fd = mkstemp(f2_fn_tpl);

  std::FILE *f2 = fdopen(f2_fd, "w");
  std::fputs(cfg_f2, f2);
  std::rewind(f2);

  auto cfg_f1 = fmt::format("subval = \"@include {}\"\n", f2_fn_tpl);

  std::FILE *f1 = std::tmpfile();
  std::fputs(cfg_f1.c_str(), f1);
  std::rewind(f1);

  auto env = fmt::format("INCLUDE_FILE={}", f2_fn_tpl).c_str();
  putenv((char *)env);

  auto c = Config();

  auto *r = c.load(f1);
  cr_assert_not_null(r);

  auto *j = json_object_get(r, "subval");
  cr_assert_not_null(j);

  auto *j2 = json_object_get(j, "magic");
  cr_assert_not_null(j2);

  cr_assert(json_is_integer(j2));
  cr_assert_eq(json_number_value(j2), 1234);

  std::fclose(f2);
  std::remove(f2_fn_tpl);
}
