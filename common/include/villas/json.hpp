/* Json parsing and support code.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2024-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <fmt/ostream.h>
#include <nlohmann/json.hpp>

#include <villas/fs.hpp>

extern template class nlohmann::basic_json<>;

namespace villas {

using Json = nlohmann::json;
using JsonPointer = Json::json_pointer;

// forward declaration for villas/jansson.hpp compatibility header
class JanssonPtr;
void to_json(Json &json, JanssonPtr const &jansson);
void from_json(Json const &json, JanssonPtr &jansson);

struct LoadConfigFileOptions {
  bool allow_libconfig = false;
  bool allow_environment = false;
  bool allow_include = false;
};

// load a configuration file
Json load_config_file(fs::path const &path, LoadConfigFileOptions const &opts);

}; // namespace villas

// forward declaration for libjansson's json_t
struct json_t;

// convert borrowed libjansson
void to_json(villas::Json &json, json_t const *jansson);

template <> // format nlohmann::json using operator<<
struct fmt::formatter<villas::Json> : ostream_formatter {};

template <> // format json_pointer using operator<<
struct fmt::formatter<villas::JsonPointer> : ostream_formatter {};
