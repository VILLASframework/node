/* Main Unit Test entry point.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>
#include <criterion/hooks.h>
#include <criterion/internal/ordered-set.h>
#include <criterion/options.h>

#include <villas/log.hpp>

#include <spdlog/spdlog.h>

// Returns true if there is at least one enabled test in this suite
static bool suite_enabled(struct criterion_test_set *tests, const char *name) {
  FOREACH_SET(void *suite_ptr, tests->suites) {
    struct criterion_suite_set *suite = (struct criterion_suite_set *)suite_ptr;

    if (!strcmp(suite->suite.name, name)) {
      FOREACH_SET(void *test_ptr, suite->tests) {
        struct criterion_test *test = (struct criterion_test *)test_ptr;

        if (!test->data->disabled)
          return true;
      }
    }
  }

  return false;
}

// Limit number of parallel jobs to 1 in case we use the FPGA
ReportHook(PRE_ALL)(struct criterion_test_set *tests) {
  if (suite_enabled(tests, "fpga")) {
    auto logger = villas::Log::get("unittest");

    logger->info("FPGA tests enabled. Only 1 job is executed in parallel!.");
    criterion_options.jobs = 1;
  }
}

int main(int argc, char *argv[]) {
  int ret;

  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%T] [%l] [%n] %v");

  // Run criterion tests
  auto tests = criterion_initialize();

  ret = criterion_handle_args(argc, argv, true);
  if (ret)
    ret = !criterion_run_all_tests(tests);

  criterion_finalize(tests);

  return ret;
}
