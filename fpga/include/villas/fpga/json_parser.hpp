/*  Wrapper for Jannson library to process Json files in OOP.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2025 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

#include <jansson.h>
#include <spdlog/common.h>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>

class JsonParser {
private:
  inline static auto logger = villas::Log::get("Json Parser");

public:
  json_t *json;

public:
  JsonParser(json_t *json) : json(json) {}

  JsonParser(const std::string &configFilePath) {
    FILE *f = fopen(configFilePath.c_str(), "r");
    if (!f)
      throw villas::RuntimeError("Cannot open config file: {}", configFilePath);

    this->json = json_loadf(f, 0, nullptr);
    if (!json) {
      logger->error("Cannot parse JSON config");
      fclose(f);
      throw villas::RuntimeError("Cannot parse JSON config");
    }

    fclose(f);
  }

  ~JsonParser() { json_decref(json); }

  json_t *get(const std::string &key) {
    json_t *result = json_object_get(this->json, key.c_str());
    if (result == nullptr) {
      logger->error("No section {} found in config", key);
      exit(1);
    }
    return result;
  }
};
