/* Helpers for configuration parsers.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <regex>

#include <jansson.h>

#include <villas/node/config.hpp>
#include <villas/sample.hpp>

#ifdef WITH_CONFIG
#include <libconfig.h>
#endif

namespace villas {
namespace node {

#ifdef WITH_CONFIG

// Convert a libconfig object to a jansson object
json_t *config_to_json(struct config_setting_t *cfg);

// Convert a jansson object into a libconfig object.
int json_to_config(json_t *json, struct config_setting_t *parent);

#endif // WITH_CONFIG

int json_object_extend_str(json_t *orig, const char *str);

void json_object_extend_key_value(json_t *obj, const char *key,
                                  const char *value);

void json_object_extend_key_value_token(json_t *obj, const char *key,
                                        const char *value);

// Merge two JSON objects recursively.
int json_object_extend(json_t *orig, json_t *merge);

json_t *json_load_cli(int argc, const char *argv[]);

template <typename Duration = std::chrono::nanoseconds>
Duration parse_duration(const std::string &input) {
  using namespace std::literals::chrono_literals;

  // Map unit strings to their corresponding chrono durations
  static const std::unordered_map<std::string, std::chrono::nanoseconds> unit_map = {
      {"d", 24h},  // days
      {"h", 1h},   // hours
      {"m", 1min}, // minutes
      {"s", 1s},   // seconds
      {"ms", 1ms}, // milliseconds
      {"us", 1us}, // microseconds
      {"ns", 1ns}  // nanoseconds
  };

  std::regex token_re(R"((\d+)([a-z]+))", std::regex::icase);
  auto begin = std::sregex_iterator(input.begin(), input.end(), token_re);
  auto end = std::sregex_iterator();

  std::chrono::nanoseconds total_duration{0};

  for (auto i = begin; i != end; ++i) {
    std::smatch match = *i;
    std::string number_str = match[1];
    std::string unit_str = match[2];

    // Convert unit to lowercase for matching
    for (auto &c : unit_str)
      c = std::tolower(c);

    auto it = unit_map.find(unit_str);
    if (it == unit_map.end()) {
      throw std::invalid_argument("Unknown duration unit: " + unit_str);
    }

    // Parse the number and multiply by the unit duration
    uint64_t number = std::stoull(number_str);

    // Add to total duration
    total_duration += it->second * number;
  }

  return std::chrono::duration_cast<Duration>(total_duration);
}

template <typename Duration = std::chrono::nanoseconds>
Duration parse_duration(json_t *json) {
  switch (json_typeof(json)) {
  case JSON_INTEGER: {
    int64_t value = json_integer_value(json);
    if (value < 0) {
        throw ConfigError(json, "duration-negative", "Negative duration value");
    }

    return Duration(value);
  }

  case JSON_STRING: {
    const char *str = json_string_value(json);
    if (!str) {
        throw ConfigError(json, "duration-string", "Invalid string value for duration");
    }

    return parse_duration<Duration>(std::string(str));
  }

  default:
    throw ConfigError(json, "duration", "Expected a string or integer for duration");
  }
}

} // namespace node
} // namespace villas
