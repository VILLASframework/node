// SPDX-FileCopyrightText: 2014-2025 The VILLASframework Authors
// SPDX-License-Identifier: Apache-2.0
// Generated file — do not edit
#pragma once

#include <concepts>
#include <functional>
#include <vector>

#include <nlohmann/json-schema.hpp>

#include <villas/json.hpp>

namespace villas::node {

using JsonUri = nlohmann::json_uri;

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

Json const &bundled_schemas();

struct JsonSchemaValidateOptions {
  bool apply_migrations = false;
  bool apply_defaults = false;
};

class JsonSchema {
  Json json_;
  nlohmann::json_schema::json_validator validator_;

public:
  explicit JsonSchema(Json const &schema)
      : json_(schema),
        validator_(schema, nullptr,
                   nlohmann::json_schema::default_string_format_check) {}

  void validate(Json &instance,
                JsonSchemaValidateOptions const &opts = {}) const;
};

} // namespace villas::node
