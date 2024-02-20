/* Signal types.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <villas/signal_type.hpp>

using namespace villas::node;

enum SignalType villas::node::signalTypeFromString(const std::string &str) {
  if (str == "boolean")
    return SignalType::BOOLEAN;
  else if (str == "complex")
    return SignalType::COMPLEX;
  else if (str == "float")
    return SignalType::FLOAT;
  else if (str == "integer")
    return SignalType::INTEGER;
  else
    return SignalType::INVALID;
}

enum SignalType villas::node::signalTypeFromFormatString(char c) {
  switch (c) {
  case 'f':
    return SignalType::FLOAT;

  case 'i':
    return SignalType::INTEGER;

  case 'c':
    return SignalType::COMPLEX;

  case 'b':
    return SignalType::BOOLEAN;

  default:
    return SignalType::INVALID;
  }
}

std::string villas::node::signalTypeToString(enum SignalType fmt) {
  switch (fmt) {
  case SignalType::BOOLEAN:
    return "boolean";

  case SignalType::COMPLEX:
    return "complex";

  case SignalType::FLOAT:
    return "float";

  case SignalType::INTEGER:
    return "integer";

  case SignalType::INVALID:
    return "invalid";
  }

  return nullptr;
}

enum SignalType villas::node::signalTypeDetect(const std::string &val) {
  int len;

  if (val.find('i') != std::string::npos)
    return SignalType::COMPLEX;

  if (val.find(',') != std::string::npos)
    return SignalType::FLOAT;

  len = val.size();
  if (len == 1 && (val[0] == '1' || val[0] == '0'))
    return SignalType::BOOLEAN;

  return SignalType::INTEGER;
}
