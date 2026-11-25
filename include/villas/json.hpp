/* Json parsing and support code.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2024-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cassert>
#include <concepts>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <nlohmann/json.hpp>

#include <villas/fs.hpp>

extern template class nlohmann::basic_json<>;

namespace villas {

using Json = nlohmann::json;
using JsonPointer = Json::json_pointer;

struct JsonDiagnostic {
  JsonPointer pointer;
  std::string message;
};

class JsonError final : public std::exception {
  std::vector<JsonDiagnostic> diagnostics_;

public:
  using value_type = decltype(diagnostics_)::value_type;

  explicit JsonError(std::vector<JsonDiagnostic> diagnostics)
      : diagnostics_(std::move(diagnostics)) {
    assert(not diagnostics_.empty());
  }

  explicit JsonError(JsonDiagnostic diagnostic)
      : JsonError(std::vector{std::move(diagnostic)}) {}

  template <typename... Args>
  JsonError(JsonPointer pointer, fmt::format_string<Args...> message,
            Args &&...args)
      : JsonError(JsonDiagnostic{
            .pointer = std::move(pointer),
            .message = fmt::format(message, std::forward<Args>(args)...),
        }) {}

  void push_back(JsonDiagnostic diagnostic) {
    diagnostics_.push_back(std::move(diagnostic));
  }

  auto what() const noexcept -> char const * override {
    return diagnostics_.front().message.c_str();
  }

  auto begin() const noexcept -> decltype(diagnostics_)::const_iterator {
    return diagnostics_.begin();
  }

  auto end() const noexcept -> decltype(diagnostics_)::const_iterator {
    return diagnostics_.end();
  }

  static auto with_parent(JsonPointer parent, JsonError error) -> JsonError {
    auto &diagnostics = error.diagnostics_;

    for (auto &diagnostic : diagnostics) {
      diagnostic.pointer = parent / std::move(diagnostic.pointer);
    }

    return JsonError(std::move(diagnostics));
  }

  template <typename Fn, typename... Args>
    requires std::invocable<Fn, Args...>
  static auto context(JsonPointer pointer, Fn &&fn, Args &&...args)
      -> std::invoke_result_t<Fn, Args...> {
    try {
      return std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
    } catch (JsonError &error) {
      throw JsonError::with_parent(std::move(pointer), std::move(error));
    }
  }
};

// forward declaration for villas/jansson.hpp compatibility header
class JanssonPtr;
void to_json(Json &json, JanssonPtr const &jansson);
void from_json(Json const &json, JanssonPtr &jansson);

struct LoadConfigFileOptions {
  bool allow_libconfig = false;
  bool allow_environment = false;
  bool allow_include = false;
  bool allow_comments = false;
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
