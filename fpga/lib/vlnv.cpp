/* Vendor, Library, Name, Version (VLNV) tag
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sstream>
#include <string>

#include <villas/fpga/vlnv.hpp>

using namespace villas::fpga;

bool Vlnv::operator==(const Vlnv &other) const {
  // If a field is empty, it means wildcard matching everything
  const bool vendorWildcard = vendor.empty() or other.vendor.empty();
  const bool libraryWildcard = library.empty() or other.library.empty();
  const bool nameWildcard = name.empty() or other.name.empty();
  const bool versionWildcard = version.empty() or other.version.empty();

  const bool vendorMatch = vendorWildcard or vendor == other.vendor;
  const bool libraryMatch = libraryWildcard or library == other.library;
  const bool nameMatch = nameWildcard or name == other.name;
  const bool versionMatch = versionWildcard or version == other.version;

  return vendorMatch and libraryMatch and nameMatch and versionMatch;
}

void Vlnv::parseFromString(std::string vlnv) {
  // Tokenize by delimiter
  std::stringstream sstream(vlnv);
  std::getline(sstream, vendor, delimiter);
  std::getline(sstream, library, delimiter);
  std::getline(sstream, name, delimiter);
  std::getline(sstream, version, delimiter);

  // Represent wildcard internally as empty string
  if (vendor == "*")
    vendor = "";
  if (library == "*")
    library = "";
  if (name == "*")
    name = "";
  if (version == "*")
    version = "";
}

std::string Vlnv::toString() const {
  std::stringstream stream;
  std::string string;

  stream << *this;
  stream >> string;

  return string;
}
