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

} // namespace villas
