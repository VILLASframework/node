/* Signal meta data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <jansson.h>

#include <villas/signal_data.hpp>
#include <villas/signal_type.hpp>

// "I" defined by complex.h collides with a define in OpenSSL
#undef I

namespace villas {
namespace node {

/* Signal descriptor.
 *
 * This data structure contains meta data about samples values in struct Sample::data
 */
class Signal {

public:
  using Ptr = std::shared_ptr<Signal>;

  std::string name; // The name of the signal.
  std::string unit; // The unit of the signal.

  union SignalData init; // The initial value of the signal.
  enum SignalType type;

  // Initialize a signal with default values.
  Signal(const std::string &n = "", const std::string &u = "",
         enum SignalType t = SignalType::INVALID);

  // Parse signal description.
  int parse(json_t *json);

  virtual std::string toString(const union SignalData *d = nullptr) const;

  // Produce JSON representation of signal.
  virtual json_t *toJson() const;

  bool isNext(const Signal &sig);

  static Signal::Ptr fromJson(json_t *json);
};

} // namespace node
} // namespace villas
