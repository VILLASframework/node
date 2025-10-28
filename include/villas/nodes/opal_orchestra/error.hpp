/* Helpers for RTAPI error handling.
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

extern "C" {
#include <RTAPI.h>
}

#include <villas/exceptions.hpp>

namespace villas {
namespace node {
namespace orchestra {

class RTError : public RuntimeError {

public:
  template <typename... Args>
  RTError(RTAPIReturn_t rc, fmt::format_string<Args...> fmt, Args &&...args)
      : RuntimeError("{}: {}", fmt::format(fmt, std::forward<Args>(args)...),
                     RTGetErrorMessage(rc)),
        returnCode(rc) {}

  RTAPIReturn_t returnCode;
};

} // namespace orchestra
} // namespace node
} // namespace villas
