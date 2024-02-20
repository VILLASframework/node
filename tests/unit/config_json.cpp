/* Unit tests libconfig to jansson converters.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>

#ifdef WITH_CONFIG

#include <jansson.h>
#include <libconfig.h>

#include <villas/config_helper.hpp>
#include <villas/utils.hpp>

using namespace villas::node;

const char *cfg_example = "test : \n"
                          "{\n"
                          "  hallo = 1L;\n"
                          "};\n"
                          "liste = ( 1.1, 2L, 3L, 4L, \n"
                          "  {\n"
                          "    objekt : \n"
                          "    {\n"
                          "      key = \"value\";\n"
                          "    };\n"
                          "  } );\n";

const char *json_example = "{\n"
                           "  \"test\": {\n"
                           "    \"hallo\": 1\n"
                           "  },\n"
                           "  \"liste\": [\n"
                           "    1.1000000000000001,\n"
                           "    2,\n"
                           "    3,\n"
                           "    4,\n"
                           "    {\n"
                           "      \"objekt\": {\n"
                           "        \"key\": \"value\"\n"
                           "      }\n"
                           "    }\n"
                           "  ]\n"
                           "}";

// cppcheck-suppress syntaxError
Test(config, config_to_json) {
  int ret;
  config_t cfg;
  config_setting_t *cfg_root;
  json_t *json;

  config_init(&cfg);

  ret = config_read_string(&cfg, cfg_example);
  cr_assert_eq(ret, CONFIG_TRUE);

  cfg_root = config_root_setting(&cfg);

  json = config_to_json(cfg_root);
  cr_assert_not_null(json);

  char *str = json_dumps(json, JSON_INDENT(2));

  //printf("%s\n", str);

  json_decref(json);

  cr_assert_str_eq(str, json_example);

  config_destroy(&cfg);
}

Test(config, json_to_config) {
  config_t cfg;
  config_setting_t *cfg_root;
  json_t *json;

  // For config_write()
  FILE *f;
  char str[1024];

  config_init(&cfg);

  cfg_root = config_root_setting(&cfg);

  json = json_loads(json_example, 0, nullptr);
  cr_assert_not_null(json);

  json_to_config(json, cfg_root);

  //config_write(&cfg, stdout);

  f = fmemopen(str, sizeof(str), "w+");
  config_write(&cfg, f);
  fclose(f);

  cr_assert_str_eq(str, cfg_example);

  json_decref(json);
}

#endif // WITH_CONFIG
