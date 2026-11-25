// SPDX-FileCopyrightText: 2014-2025 The VILLASframework Authors
// SPDX-License-Identifier: Apache-2.0
// Generated file — do not edit
#pragma once

#include <vector>

#include <nlohmann/json-schema.hpp>

#include <villas/json.hpp>

namespace villas::node {

using JsonUri = nlohmann::json_uri;

Json const &bundled_schemas();

class JsonSchema {
  Json json_;
  nlohmann::json_schema::json_validator validator_;

public:
  explicit JsonSchema(Json const &schema)
      : json_(schema),
        validator_(schema, nullptr,
                   nlohmann::json_schema::default_string_format_check) {}

  Json const &json() const { return json_; }

  Json validate(Json const &json) const {
    struct final : nlohmann::json_schema::error_handler {
      std::vector<JsonDiagnostic> diagnostics{};

      void error(JsonPointer const &pointer, Json const &instance,
                 std::string const &message) override {
        diagnostics.emplace_back(pointer, message);
      }
    } error_handler;

    if (auto default_values = validator_.validate(json, error_handler);
        error_handler.diagnostics.empty())
      return default_values;
    else
      throw JsonError(std::move(error_handler.diagnostics));
  }
};

} // namespace villas::node
