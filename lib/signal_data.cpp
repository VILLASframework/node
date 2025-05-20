/* Signal data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cinttypes>
#include <complex>
#include <cstring>

#include <villas/exceptions.hpp>
#include <villas/signal_data.hpp>
#include <villas/signal_type.hpp>

using namespace villas::node;

void SignalData::set(enum SignalType type, double val) {
  switch (type) {
  case SignalType::BOOLEAN:
    b = val;
    break;

  case SignalType::FLOAT:
    f = val;
    break;

  case SignalType::INTEGER:
    i = val;
    break;

  case SignalType::COMPLEX:
    z = val;
    break;

  case SignalType::INVALID:
    *this = nan();
    break;
  }
}

SignalData SignalData::cast(enum SignalType from, enum SignalType to) const {
  if (from == to) {
    return *this;
  }

  switch (to) {
  case SignalType::BOOLEAN:
    switch (from) {
    case SignalType::INTEGER:
      return bool(i);

    case SignalType::FLOAT:
      return bool(f);

    case SignalType::COMPLEX:
      return std::real(z) != 0 && std::imag(z) != 0;

    default:
      break;
    }
    break;

  case SignalType::INTEGER:
    switch (from) {
    case SignalType::BOOLEAN:
      return int64_t(b);

    case SignalType::FLOAT:
      return int64_t(f);

    case SignalType::COMPLEX:
      return int64_t(z.real());

    default:
      break;
    }
    break;

  case SignalType::FLOAT:
    switch (from) {
    case SignalType::BOOLEAN:
      return double(b);

    case SignalType::INTEGER:
      return double(i);

    case SignalType::COMPLEX:
      return std::real(z);

    default:
      break;
    }
    break;

  case SignalType::COMPLEX:
    switch (from) {
    case SignalType::BOOLEAN:
      return std::complex<float>(b, 0);

    case SignalType::INTEGER:
      return std::complex<float>(i, 0);

    case SignalType::FLOAT:
      return std::complex<float>(f, 0);

    default:
      break;
    }
    break;

  default:
    break;
  }

  throw RuntimeError("Failed to cast signal data from {} to {}",
                     signalTypeToString(from), signalTypeToString(to));
}

int SignalData::parseString(enum SignalType type, const char *ptr, char **end) {
  switch (type) {
  case SignalType::FLOAT:
    f = strtod(ptr, end);
    break;

  case SignalType::INTEGER:
    i = strtol(ptr, end, 10);
    break;

  case SignalType::BOOLEAN:
    b = strtol(ptr, end, 10);
    break;

  case SignalType::COMPLEX: {
    float real, imag = 0;

    real = strtod(ptr, end);
    if (*end == ptr)
      return -1;

    ptr = *end;

    if (*ptr == 'i' || *ptr == 'j') {
      imag = real;
      real = 0;

      (*end)++;
    } else if (*ptr == '-' || *ptr == '+') {
      imag = strtod(ptr, end);
      if (*end == ptr)
        return -1;

      if (**end != 'i' && **end != 'j')
        return -1;

      (*end)++;
    }

    z = std::complex<float>(real, imag);
    break;
  }

  case SignalType::INVALID:
    return -1;
  }

  return 0;
}

int SignalData::parseJson(enum SignalType type, json_t *json) {
  int ret;

  switch (type) {
  case SignalType::FLOAT:
    if (!json_is_number(json))
      return -1;

    f = json_number_value(json);
    break;

  case SignalType::INTEGER:
    if (!json_is_integer(json))
      return -1;

    i = json_integer_value(json);
    break;

  case SignalType::BOOLEAN:
    if (!json_is_boolean(json))
      return -1;

    b = json_boolean_value(json);
    break;

  case SignalType::COMPLEX: {
    double real, imag;

    json_error_t err;
    ret = json_unpack_ex(json, &err, 0, "{ s: F, s: F }", "real", &real, "imag",
                         &imag);
    if (ret)
      return -1;

    z = std::complex<float>(real, imag);
    break;
  }

  case SignalType::INVALID:
    return -2;
  }

  return 0;
}

json_t *SignalData::toJson(enum SignalType type) const {
  switch (type) {
  case SignalType::INTEGER:
    return json_integer(i);

  case SignalType::FLOAT:
    return json_real(f);

  case SignalType::BOOLEAN:
    return json_boolean(b);

  case SignalType::COMPLEX:
    return json_pack("{ s: f, s: f }", "real", z.real(), "imag", z.imag());

  case SignalType::INVALID:
    return nullptr;
  }

  return nullptr;
}

int SignalData::printString(enum SignalType type, char *buf, size_t len,
                            int precision) const {
  switch (type) {
  case SignalType::FLOAT:
    return snprintf(buf, len, "%.*f", precision, f);

  case SignalType::INTEGER:
    return snprintf(buf, len, "%" PRIi64, i);

  case SignalType::BOOLEAN:
    return snprintf(buf, len, "%u", b);

  case SignalType::COMPLEX:
    return snprintf(buf, len, "%.*f%+.*fi", precision, z.real(), precision,
                    z.imag());

  default:
    return snprintf(buf, len, "<?>");
  }
}

std::string SignalData::toString(enum SignalType type, int precision) const {
  char buf[128];

  printString(type, buf, sizeof(buf), precision);

  return buf;
}
