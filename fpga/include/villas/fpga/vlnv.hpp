/* Vendor, Library, Name, Version (VLNV) tag.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include <fmt/ostream.h>

#include <villas/config.hpp>

namespace villas {
namespace fpga {

class Vlnv {
public:
  static constexpr char delimiter = ':';

  Vlnv() : vendor(""), library(""), name(""), version("") {}

  Vlnv(const std::string &s) { parseFromString(s); }

  static Vlnv getWildcard() { return Vlnv(); }

  std::string toString() const;

  bool operator==(const Vlnv &other) const;

  bool operator!=(const Vlnv &other) const { return !(*this == other); }

  friend std::ostream &operator<<(std::ostream &stream, const Vlnv &vlnv) {
    return stream << (vlnv.vendor.empty() ? "*" : vlnv.vendor) << ":"
                  << (vlnv.library.empty() ? "*" : vlnv.library) << ":"
                  << (vlnv.name.empty() ? "*" : vlnv.name) << ":"
                  << (vlnv.version.empty() ? "*" : vlnv.version);
  }

private:
  void parseFromString(std::string vlnv);

  std::string vendor;
  std::string library;
  std::string name;
  std::string version;
};

} // namespace fpga
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::fpga::Vlnv> : public fmt::ostream_formatter {};
#endif
