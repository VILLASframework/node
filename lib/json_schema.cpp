/* Json schema support code.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ranges>
#include <vector>

#include <villas/exceptions.hpp>
#include <villas/format.hpp>
#include <villas/hook.hpp>
#include <villas/node.hpp>
#include <villas/node/json_schema.hpp>
#include <villas/plugin.hpp>

using namespace villas;
using namespace villas::node;

static void walk_schema(Json &instance, JsonPointer const &ptr,
                        Json const &schema,
                        JsonSchemaValidateOptions const &opts);

template <typename T>
static void walk_plugin(Json &instance, JsonPointer const &ptr,
                        std::string const &property,
                        JsonSchemaValidateOptions const &opts) {
  Json name;
  if (instance.is_string()) {
    name = instance;
    instance = Json::object({{property, name}});
  } else if (instance.is_object() and instance.contains(property)) {
    name = instance[property];
  } else {
    throw JsonError({
        .pointer = ptr,
        .message = fmt::format("unknown {}", property),
    });
  }

  auto factory = plugin::registry->lookup<T>(name);
  if (not factory) {
    throw JsonError({
        .pointer = instance.is_string() ? ptr : ptr / property,
        .message = fmt::format("unknown {} {}", property, name),
    });
  }

  if (opts.apply_migrations) {
    Json migration_patch =
        JsonError::context(ptr, [&]() { return factory->migrate(instance); });

    auto logger = factory->getLogger();
    for (auto const &op : migration_patch)
      logger->warn("migrate[{}]: {}", ptr, op);

    instance.patch_inplace(migration_patch);
  }

  JsonError::context(ptr,
                     [&]() { factory->getSchema().validate(instance, opts); });
}

static void walk_schema(Json &instance, JsonPointer const &ptr,
                        Json const &schema,
                        JsonSchemaValidateOptions const &opts) {
  if (not schema.is_object())
    return;

  std::vector<JsonDiagnostic> diagnostics;

  if (auto discriminator = schema.find("discriminator");
      discriminator != schema.end()) {
    if (auto plugin = discriminator->find("x-villas-plugin");
        plugin != discriminator->end()) {
      auto const &property =
          discriminator->at("propertyName").get_ref<Json::string_t const &>();
      try {
        if (*plugin == "node")
          walk_plugin<NodeFactory>(instance, ptr, property, opts);
        else if (*plugin == "hook")
          walk_plugin<HookFactory>(instance, ptr, property, opts);
        else if (*plugin == "format")
          walk_plugin<FormatFactory>(instance, ptr, property, opts);
        else
          throw RuntimeError("invalid x-villas-plugin annotation {} in schema",
                             *plugin);
      } catch (JsonError &error) {
        diagnostics.insert(diagnostics.end(), error.begin(), error.end());
      }
    }
  }

  auto properties = schema.value("properties", Json::object());
  auto additionalProperties = schema.find("additionalProperties");
  if (instance.is_object() and
      (not properties.empty() or additionalProperties != schema.end())) {
    for (auto const &[property, value] : instance.items()) {
      try {
        if (auto subschema = properties.find(property);
            subschema != properties.end())
          walk_schema(value, ptr / property, *subschema, opts);
        else if (additionalProperties != schema.end())
          walk_schema(value, ptr / property, *additionalProperties, opts);
      } catch (JsonError &error) {
        diagnostics.insert(diagnostics.end(), error.begin(), error.end());
      }
    }
  }

  if (auto items = schema.find("items");
      instance.is_array() and items != schema.end()) {
    if (items->is_array()) {
      auto additionalItems = schema.find("additionalItems");
      for (auto const index : std::views::iota(size_t(0), instance.size())) {
        try {
          if (index < items->size())
            walk_schema(instance[index], ptr / index, (*items)[index], opts);
          else if (additionalItems != schema.end())
            walk_schema(instance[index], ptr / index, *additionalItems, opts);
          else
            break;
        } catch (JsonError &error) {
          diagnostics.insert(diagnostics.end(), error.begin(), error.end());
        }
      }
    } else {
      for (auto const index : std::views::iota(size_t(0), instance.size())) {
        try {
          walk_schema(instance[index], ptr / index, *items, opts);
        } catch (JsonError &error) {
          diagnostics.insert(diagnostics.end(), error.begin(), error.end());
        }
      }
    }
  }

  if (auto subschemas = schema.find("allOf"); subschemas != schema.end()) {
    for (auto const &subschema : *subschemas) {
      try {
        walk_schema(instance, ptr, subschema, opts);
      } catch (JsonError &error) {
        diagnostics.insert(diagnostics.end(), error.begin(), error.end());
      }
    }
  }

  if (not diagnostics.empty())
    throw JsonError(diagnostics);
}

void JsonSchema::validate(Json &instance,
                          JsonSchemaValidateOptions const &opts) const {
  struct final : nlohmann::json_schema::error_handler {
    std::vector<JsonDiagnostic> diagnostics{};

    void error(JsonPointer const &pointer, Json const &instance,
               std::string const &message) override {
      diagnostics.emplace_back(pointer, message);
    }
  } error_handler;

  auto default_values = validator_.validate(instance, error_handler);
  if (not error_handler.diagnostics.empty())
    throw JsonError(std::move(error_handler.diagnostics));

  if (opts.apply_defaults)
    instance.patch_inplace(default_values);

  walk_schema(instance, JsonPointer{}, json_, opts);
}
