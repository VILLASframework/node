/* Unit tests for config features.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <criterion/criterion.h>
#include <criterion/internal/assert.h>

#include <villas/config_class.hpp>
#include <villas/utils.hpp>

using namespace std::string_view_literals;
using namespace villas::node;
using FileDeleter = decltype([](std::FILE *f) { std::fclose(f); });
using FilePtr = std::unique_ptr<std::FILE, FileDeleter>;

constexpr auto fileNameTemplate = "villas.unit-test.XXXXXX.conf"sv;
constexpr auto fileNameSuffix = ".conf"sv;

// cppcheck-suppress syntaxError
Test(config, env) {
  auto configString = R"libconfig(
    test = "${MY_ENV_VAR}"
  )libconfig";

  auto configPathTemplate =
      std::string(fs::temp_directory_path() / fileNameTemplate);
  auto configFd =
      ::mkstemps(configPathTemplate.data(), fileNameSuffix.length());
  auto configFile = FilePtr(::fdopen(configFd, "w"));
  auto configPath = fs::path(configPathTemplate);
  std::fputs(configString, configFile.get());
  configFile.reset();

  auto config = Config();
  ::setenv("MY_ENV_VAR", "mobydick", true);

  auto *root = config.load(fs::path(configPath));
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

  auto incPathTemplate =
      std::string(fs::temp_directory_path() / fileNameTemplate);
  auto incFd = ::mkstemps(incPathTemplate.data(), fileNameSuffix.length());
  cr_assert(incFd >= 0);
  auto incFile = FilePtr(::fdopen(incFd, "w"));
  cr_assert_not_null(incFile);
  auto incPath = fs::path(incPathTemplate);
  std::fputs(incString, incFile.get());
  incFile.reset();

  auto configString = fmt::format(R"libconfig(
    subval = {{ @include "{}" }}
  )libconfig",
                                  incPath.string());
  auto configPathTemplate =
      std::string(fs::temp_directory_path() / fileNameTemplate);
  auto configFd =
      ::mkstemps(configPathTemplate.data(), fileNameSuffix.length());
  cr_assert(configFd >= 0);
  auto configFile = FilePtr(::fdopen(configFd, "w"));
  cr_assert_not_null(configFile);
  auto configPath = fs::path(configPathTemplate);
  std::fputs(configString.c_str(), configFile.get());
  configFile.reset();

  auto config = Config();
  auto *root = config.load(configPath);
  cr_assert_not_null(root);

  auto *subval = json_object_get(root, "subval");
  cr_assert_not_null(subval);

  auto *magic = json_object_get(subval, "magic");
  cr_assert_not_null(magic);
  cr_assert(json_is_integer(magic));
  cr_assert_eq(json_number_value(magic), 1234);
}
