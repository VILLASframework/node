/* Version.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

namespace villas {
namespace utils {

class Version {

protected:
  int components[3];

  static int cmp(const Version &lhs, const Version &rhs);

public:
  // Parse a dotted version string.
  Version(const std::string &s);

  Version(int maj, int min = 0, int pat = 0);

  inline bool operator==(const Version &rhs) { return cmp(*this, rhs) == 0; }

  inline bool operator!=(const Version &rhs) { return cmp(*this, rhs) != 0; }

  inline bool operator<(const Version &rhs) { return cmp(*this, rhs) < 0; }

  inline bool operator>(const Version &rhs) { return cmp(*this, rhs) > 0; }

  inline bool operator<=(const Version &rhs) { return cmp(*this, rhs) <= 0; }

  inline bool operator>=(const Version &rhs) { return cmp(*this, rhs) >= 0; }
};

} // namespace utils
} // namespace villas
