/* Signal data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cinttypes>
#include <cstring>

#include <villas/signal_data.hpp>
#include <villas/signal_type.hpp>

using namespace villas::node;

void SignalData::set(enum SignalType type, double val) {
  switch (type) {
  case SignalType::BOOLEAN:
    this->b = val;
    break;

  case SignalType::FLOAT:
    this->f = val;
    break;

  case SignalType::INTEGER:
    this->i = val;
    break;

  case SignalType::COMPLEX:
    this->z = val;
    break;

  case SignalType::INVALID:
    *this = nan();
    break;
  }
}

SignalData SignalData::cast(enum SignalType from, enum SignalType to) const {
  SignalData n = *this;

  switch (to) {
  case SignalType::BOOLEAN:
    switch (from) {
    case SignalType::INTEGER:
      n.b = this->i;
      break;

    case SignalType::FLOAT:
      n.b = this->f;
      break;

    case SignalType::COMPLEX:
      n.b = std::real(this->z);
      break;

    case SignalType::INVALID:
    case SignalType::BOOLEAN:
      break;
    }
    break;

  case SignalType::INTEGER:
    switch (from) {
    case SignalType::BOOLEAN:
      n.i = this->b;
      break;

    case SignalType::FLOAT:
      n.i = this->f;
      break;

    case SignalType::COMPLEX:
      n.i = std::real(this->z);
      break;

    case SignalType::INVALID:
    case SignalType::INTEGER:
      break;
    }
    break;

  case SignalType::FLOAT:
    switch (from) {
    case SignalType::BOOLEAN:
      n.f = this->b;
      break;

    case SignalType::INTEGER:
      n.f = this->i;
      break;

    case SignalType::COMPLEX:
      n.f = std::real(this->z);
      break;

    case SignalType::INVALID:
    case SignalType::FLOAT:
      break;
    }
    break;

  case SignalType::COMPLEX:
    switch (from) {
    case SignalType::BOOLEAN:
      n.z = this->b;
      break;

    case SignalType::INTEGER:
      n.z = this->i;
      break;

    case SignalType::FLOAT:
      n.z = this->f;
      break;

    case SignalType::INVALID:
    case SignalType::COMPLEX:
      break;
    }
    break;

  default: {
  }
  }

  return n;
}

int SignalData::parseString(enum SignalType type, const char *ptr, char **end) {
  switch (type) {
  case SignalType::FLOAT:
    this->f = strtod(ptr, end);
    break;

  case SignalType::INTEGER:
    this->i = strtol(ptr, end, 10);
    break;

  case SignalType::BOOLEAN:
    this->b = strtol(ptr, end, 10);
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

    this->z = std::complex<float>(real, imag);
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

    this->f = json_number_value(json);
    break;

  case SignalType::INTEGER:
    if (!json_is_integer(json))
      return -1;

    this->i = json_integer_value(json);
    break;

  case SignalType::BOOLEAN:
    if (!json_is_boolean(json))
      return -1;

    this->b = json_boolean_value(json);
    break;

  case SignalType::COMPLEX: {
    double real, imag;

    json_error_t err;
    ret = json_unpack_ex(json, &err, 0, "{ s: F, s: F }", "real", &real, "imag",
                         &imag);
    if (ret)
      return -1;

    this->z = std::complex<float>(real, imag);
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
    return json_integer(this->i);

  case SignalType::FLOAT:
    return json_real(this->f);

  case SignalType::BOOLEAN:
    return json_boolean(this->b);

  case SignalType::COMPLEX:
    return json_pack("{ s: f, s: f }", "real", std::real(this->z), "imag",
                     std::imag(this->z));

  case SignalType::INVALID:
    return nullptr;
  }

  return nullptr;
}

int SignalData::printString(enum SignalType type, char *buf, size_t len,
                            int precision) const {
  switch (type) {
  case SignalType::FLOAT:
    return snprintf(buf, len, "%.*f", precision, this->f);

  case SignalType::INTEGER:
    return snprintf(buf, len, "%" PRIi64, this->i);

  case SignalType::BOOLEAN:
    return snprintf(buf, len, "%u", this->b);

  case SignalType::COMPLEX:
    return snprintf(buf, len, "%.*f%+.*fi", precision, std::real(this->z),
                    precision, std::imag(this->z));

  default:
    return snprintf(buf, len, "<?>");
  }
}

std::string SignalData::toString(enum SignalType type, int precission) const {
  char buf[128];

  printString(type, buf, sizeof(buf), precission);

  return buf;
}
