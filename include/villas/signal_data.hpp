/* Signal data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <complex>
#include <cstdint>
#include <limits>
#include <string>

#include <jansson.h>

#include <villas/signal_type.hpp>

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
  SignalData(bool b) : b(b) {}
  SignalData(uint8_t i) : i(i) {}
  SignalData(uint16_t i) : i(i) {}
  SignalData(uint32_t i) : i(i) {}
  SignalData(uint64_t i) : i(i) {}
  SignalData(int8_t i) : i(i) {}
  SignalData(int16_t i) : i(i) {}
  SignalData(int32_t i) : i(i) {}
  SignalData(int64_t i) : i(i) {}
  SignalData(double f) : f(f) {}
  SignalData(float f) : f(f) {}
  SignalData(std::complex<float> z) : z(z) {}

  operator bool() const { return b; }
  operator uint8_t() const { return i; }
  operator uint16_t() const { return i; }
  operator uint32_t() const { return i; }
  operator uint64_t() const { return i; }
  operator int8_t() const { return i; }
  operator int16_t() const { return i; }
  operator int32_t() const { return i; }
  operator int64_t() const { return i; }
  operator double() const { return f; }
  operator float() const { return f; }
  operator std::complex<float>() const { return z; }

  static SignalData nan() { return std::numeric_limits<double>::quiet_NaN(); }

  // Convert signal data from one description/format to another.
  SignalData cast(enum SignalType type, enum SignalType to) const;

  // Set data from double
  void set(enum SignalType type, double val);

  // Print value of a signal to a character buffer.
  int printString(enum SignalType type, char *buf, size_t len,
                  int precision = 5) const;

  // Parse string to signal data.
  int parseString(enum SignalType type, const char *ptr, char **end);

  // Parse JSON value to signal data.
  int parseJson(enum SignalType type, json_t *json);

  // Convert signal data to JSON value.
  json_t *toJson(enum SignalType type) const;

  // Convert signal data to string.
  std::string toString(enum SignalType type, int precision = 5) const;
};

} // namespace node
} // namespace villas
