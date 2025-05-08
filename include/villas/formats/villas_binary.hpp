/* Message related functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdlib>

#include <villas/format.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class VillasBinaryFormat : public BinaryFormat {

protected:
  uint8_t source_index;
  bool web;
  bool validate_source_index;

public:
  VillasBinaryFormat(int fl, bool w, uint8_t sid = 0)
      : BinaryFormat(fl), source_index(sid), web(w),
        validate_source_index(false) {}

  int sscan(const char *buf, size_t len, size_t *rbytes,
            struct Sample *const smps[], unsigned cnt) override;
  int sprint(char *buf, size_t len, size_t *wbytes,
             const struct Sample *const smps[], unsigned cnt) override;

  void parse(json_t *json) override;
};

template <bool web = false>
class VillasBinaryFormatPlugin : public FormatFactory {

public:
  using FormatFactory::FormatFactory;

  Format *make() override {
    return new VillasBinaryFormat((int)SampleFlags::HAS_TS_ORIGIN |
                                      (int)SampleFlags::HAS_SEQUENCE |
                                      (int)SampleFlags::HAS_DATA,
                                  web);
  }

  /// Get plugin name
  std::string getName() const override {
    std::stringstream ss;

    ss << "villas." << (web ? "web" : "binary");

    return ss.str();
  }

  /// Get plugin description
  virtual std::string getDescription() const {
    std::stringstream ss;

    ss << "VILLAS binary network format";

    if (web)
      ss << " for WebSockets";

    return ss.str();
  }
};

} // namespace node
} // namespace villas
