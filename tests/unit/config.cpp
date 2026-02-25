/* Unit tests for config features.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <cstdlib>

#include <criterion/criterion.h>
#include <criterion/internal/assert.h>
#include <nlohmann/json.hpp>

#include <villas/fs.hpp>
#include <villas/jansson.hpp>
#include <villas/json.hpp>
#include <villas/utils.hpp>

using namespace std::string_view_literals;
using FileDeleter = decltype([](std::FILE *f) { std::fclose(f); });
using FilePtr = std::unique_ptr<std::FILE, FileDeleter>;

constexpr auto fileNameTemplate = "villas.unit-test.XXXXXX.conf"sv;
constexpr auto fileNameSuffix = ".conf"sv;

// cppcheck-suppress syntaxError
Test(config, env) {
  auto config_string = R"libconfig(
    test = "${MY_ENV_VAR}"
  )libconfig";

  auto config_path_template =
      std::string(fs::temp_directory_path() / fileNameTemplate);
  auto config_fd =
      ::mkstemps(config_path_template.data(), fileNameSuffix.length());
  auto config_file = FilePtr(::fdopen(config_fd, "w"));
  auto config_path = fs::path(config_path_template);
  std::fputs(config_string, config_file.get());
  config_file.reset();

  auto config = villas::load_config_deprecated(config_path, true, true)
                    .get<villas::JanssonPtr>();
  ::setenv("MY_ENV_VAR", "mobydick", true);

  auto *root = config.get();
  cr_assert_not_null(root);

  auto *string = json_object_get(root, "test");
  cr_assert_not_null(string);
  cr_assert(json_is_string(string));
  cr_assert_str_eq("mobydick", json_string_value(string));
}

Test(config, include) {
  auto incString = R"libconfig(
    magic = 1234
  )libconfig";

  auto inc_path_template =
      std::string(fs::temp_directory_path() / fileNameTemplate);
  auto inc_fd = ::mkstemps(inc_path_template.data(), fileNameSuffix.length());
  cr_assert(inc_fd >= 0);
  auto inc_file = FilePtr(::fdopen(inc_fd, "w"));
  cr_assert_not_null(inc_file);
  auto inc_path = fs::path(inc_path_template);
  std::fputs(incString, inc_file.get());
  inc_file.reset();

  auto config_string = fmt::format(R"libconfig(
    subval = {{ @include "{}" }}
  )libconfig",
                                   inc_path.string());
  auto config_path_template =
      std::string(fs::temp_directory_path() / fileNameTemplate);
  auto config_fd =
      ::mkstemps(config_path_template.data(), fileNameSuffix.length());
  cr_assert(config_fd >= 0);
  auto config_file = FilePtr(::fdopen(config_fd, "w"));
  cr_assert_not_null(config_file);
  auto config_path = fs::path(config_path_template);
  std::fputs(config_string.c_str(), config_file.get());
  config_file.reset();

  auto config = villas::load_config_deprecated(config_path, true, true)
                    .get<villas::JanssonPtr>();
  auto *root = config.get();
  cr_assert_not_null(root);

  auto *subval = json_object_get(root, "subval");
  cr_assert_not_null(subval);

  auto *magic = json_object_get(subval, "magic");
  cr_assert_not_null(magic);
  cr_assert(json_is_integer(magic));
  cr_assert_eq(json_number_value(magic), 1234);
}
