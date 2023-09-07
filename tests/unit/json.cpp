/* Unit tests for libjansson helpers.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <villas/config_helper.hpp>
#include <villas/utils.hpp>

#include "helpers.hpp"

using namespace villas::node;

struct param {
  char *argv[32];
  char *json;
};

// cppcheck-suppress syntaxError
ParameterizedTestParameters(json, json_load_cli) {
  const auto d = cr_strdup;

  static criterion::parameters<struct param> params = {
      // Combined long option
      {.argv = {d("dummy"), d("--option=value")},
       .json = d("{ \"option\" : \"value\" }")},
      // Separated long option
      {.argv = {d("dummy"), d("--option"), d("value")},
       .json = d("{ \"option\" : \"value\" }")},
      // All kinds of data types
      {.argv = {d("dummy"), d("--integer"), d("1"), d("--real"), d("1.1"),
                d("--bool"), d("true"), d("--null"), d("null"), d("--string"),
                d("hello world")},
       .json = d("{ \"integer\" : 1, \"real\" : 1.1, \"bool\" : true, \"null\" "
                 ": null, \"string\" : \"hello world\" }")},
      // Array generation
      {.argv = {d("dummy"), d("--bool"), d("true"), d("--bool"), d("false")},
       .json = d("{ \"bool\" : [ true, false ] }")},
      // Dots in the option name generate sub objects
      {.argv = {d("dummy"), d("--sub.option"), d("value")},
       .json = d("{ \"sub\" : { \"option\" : \"value\" } }")},
      // Nesting is possible
      {.argv = {d("dummy"), d("--sub.sub.option"), d("value")},
       .json = d("{ \"sub\" : { \"sub\" : { \"option\" : \"value\" } } }")},
      // Multiple subgroups are merged
      {.argv = {d("dummy"), d("--sub.sub.option"), d("value1"),
                d("--sub.option"), d("value2")},
       .json = d("{ \"sub\" : { \"option\" : \"value2\", \"sub\" : { "
                 "\"option\" : \"value1\" } } }")}};

  return params;
}

ParameterizedTest(struct param *p, json, json_load_cli) {
  json_error_t err;
  json_t *json, *cli;

  json = json_loads(p->json, 0, &err);
  cr_assert_not_null(json);

  int argc = 0;
  while (p->argv[argc])
    argc++;

  cli = json_load_cli(argc, (const char **)p->argv);
  cr_assert_not_null(cli);

  //json_dumpf(json, stdout, JSON_INDENT(2)); putc('\n', stdout);
  //json_dumpf(cli, stdout, JSON_INDENT(2)); putc('\n', stdout);

  cr_assert(json_equal(json, cli));
}
