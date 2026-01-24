/* Common exceptions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cerrno>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#include <fmt/core.h>
#include <jansson.h>

#include <villas/config.hpp>

namespace villas {

class SystemError : public std::system_error {

public:
  template <typename... Args>
  SystemError(fmt::format_string<Args...> what, Args &&...args)
      : std::system_error(errno, std::system_category(),
                          fmt::format(what, std::forward<Args>(args)...)) {}
};

class RuntimeError : public std::runtime_error {

public:
  template <typename... Args>
  RuntimeError(fmt::format_string<Args...> what, Args &&...args)
      : std::runtime_error(fmt::format(what, std::forward<Args>(args)...)) {}
};

class MemoryAllocationError : public RuntimeError {

public:
  MemoryAllocationError() : RuntimeError("Failed to allocate memory") {}
};

class JsonError : public std::runtime_error {

protected:
  json_error_t error;

public:
  template <typename... Args>
  JsonError(const json_t *s, const json_error_t &e,
            const std::string &what = std::string(), Args &&...args)
      : std::runtime_error(
            fmt::format("{}: {} in {}:{}:{}",
                        fmt::format(what, std::forward<Args>(args)...),
                        error.text, error.source, error.line, error.column)),
        error(e) {}
};

class ConfigError : public std::runtime_error {

protected:
  // A setting-id referencing the setting.
  std::string id;
  json_t *setting;
  json_error_t error;

  char *msg;

  std::string getMessage() const {
    std::stringstream ss;

    ss << std::runtime_error::what() << std::endl;

    if (error.position >= 0) {
      ss << std::endl;
      ss << " " << error.text << " in " << error.source << ":" << error.line
         << ":" << error.column << std::endl;
    }

    ss << std::endl
       << " Please consult the user documentation for details: " << std::endl;
    ss << "   " << docUri();

    ss << std::endl;

    return ss.str();
  }

public:
  ~ConfigError() override {
    if (msg)
      free(msg);
  }

  template <typename... Args>
  ConfigError(
      json_t *s, const std::string &i,
      fmt::format_string<Args...> what = "Failed to parse configuration",
      Args &&...args)
      : std::runtime_error(fmt::format(what, std::forward<Args>(args)...)),
        id(i), setting(s) {
    error.position = -1;

    msg = strdup(getMessage().c_str());
  }

  template <typename... Args>
  ConfigError(
      json_t *s, const json_error_t &e, const std::string &i,
      fmt::format_string<Args...> what = "Failed to parse configuration",
      Args &&...args)
      : std::runtime_error(fmt::format(what, std::forward<Args>(args)...)),
        id(i), setting(s), error(e) {
    msg = strdup(getMessage().c_str());
  }

  std::string docUri() const {
    std::string baseUri = "https://villas.fein-aachen.org/doc/jump?";

    return baseUri + id;
  }

  const char *what() const noexcept override { return msg; }
};

} // namespace villas
