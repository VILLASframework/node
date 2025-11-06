/* Helpers for RTAPI signal type conversion.
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

#include <fmt/format.h>

#include <villas/signal_data.hpp>
#include <villas/signal_type.hpp>

namespace villas {
namespace node {
namespace orchestra {

enum class SignalType {
  BOOLEAN,
  UINT8,
  UINT16,
  UINT32,
  UINT64,
  INT8,
  INT16,
  INT32,
  INT64,
  FLOAT32,
  FLOAT64,
  BUS,
};

orchestra::SignalType toOrchestraSignalType(node::SignalType t);

node::SignalType toNodeSignalType(orchestra::SignalType t);

orchestra::SignalType signalTypeFromString(const std::string &t);

std::string signalTypeToString(orchestra::SignalType t);

node::SignalData toNodeSignalData(const void *orchestraData,
                                  orchestra::SignalType orchestraType,
                                  node::SignalType &villasType);

void toOrchestraSignalData(void *orchestraData,
                           orchestra::SignalType orchestraType,
                           const SignalData &villasData,
                           node::SignalType villasType);

} // namespace orchestra
} // namespace node
} // namespace villas

template <>
struct fmt::formatter<villas::node::orchestra::SignalType>
    : formatter<string_view> {
  auto format(villas::node::orchestra::SignalType t, format_context &ctx) const
      -> format_context::iterator {
    return formatter<string_view>::format(signalTypeToString(t), ctx);
  }
};
