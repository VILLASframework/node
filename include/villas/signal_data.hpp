/* Signal data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <complex>
#include <limits>
#include <string>

#include <cstdint>

namespace villas {
namespace node {

/* A signal value.
 *
 * Data is in host endianess!
 */
union SignalData {
  double f;              // Floating point values.
  int64_t i;             // Integer values.
  bool b;                // Boolean values.
  std::complex<float> z; // Complex values.

  SignalData() : i(0) {}

  static union SignalData nan() {
    union SignalData d;

    d.f = std::numeric_limits<double>::quiet_NaN();

    return d;
  }

  bool is_nan() { return f == std::numeric_limits<double>::quiet_NaN(); }

  // Convert signal data from one description/format to another.
  SignalData cast(enum SignalType type, enum SignalType to) const;

  // Set data from double
  void set(enum SignalType type, double val);

  // Print value of a signal to a character buffer.
  int printString(enum SignalType type, char *buf, size_t len,
                  int precision = 5) const;

  int parseString(enum SignalType type, const char *ptr, char **end);

  int parseJson(enum SignalType type, json_t *json);

  json_t *toJson(enum SignalType type) const;

  std::string toString(enum SignalType type, int precision = 5) const;
};

} // namespace node
} // namespace villas
