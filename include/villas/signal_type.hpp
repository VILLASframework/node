/* Signal type.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

namespace villas {
namespace node {

enum class SignalType {
  INVALID = 0, // Signal type is invalid.
  FLOAT = 1,   // See SignalData::f
  INTEGER = 2, // See SignalData::i
  BOOLEAN = 3, // See SignalData::b
  COMPLEX = 4  // See SignalData::z
};

enum SignalType signalTypeFromString(const std::string &str);

enum SignalType signalTypeFromFormatString(char c);

std::string signalTypeToString(enum SignalType fmt);

enum SignalType signalTypeDetect(const std::string &val);

} // namespace node
} // namespace villas
