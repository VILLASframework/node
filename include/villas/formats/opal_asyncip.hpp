/* A custom format for OPAL-RTs AsyncIP example.
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

class OpalAsyncIPFormat : public BinaryFormat {

protected:
  const unsigned MAXSIZE = 64;

  struct Payload {
    int16_t dev_id;  // (2 bytes) Sender device ID
    int32_t msg_id;  // (4 bytes) Message ID
    int16_t msg_len; // (2 bytes) Message length (data only)
    double data[];   // Up to MAXSIZE doubles (8 bytes each)
  } __attribute__((packed));

  int16_t dev_id;

public:
  OpalAsyncIPFormat(int fl, uint8_t did = 0) : BinaryFormat(fl), dev_id(did) {}

  int sscan(const char *buf, size_t len, size_t *rbytes,
            struct Sample *const smps[], unsigned cnt) override;
  int sprint(char *buf, size_t len, size_t *wbytes,
             const struct Sample *const smps[], unsigned cnt) override;

  void parse(json_t *json) override;
};

class OpalAsyncIPFormatPlugin : public FormatFactory {

public:
  using FormatFactory::FormatFactory;

  Format *make() override {
    return new OpalAsyncIPFormat((int)SampleFlags::HAS_SEQUENCE |
                                 (int)SampleFlags::HAS_DATA);
  }

  // Get plugin name
  std::string getName() const override { return "opal.asyncip"; }

  // Get plugin description
  std::string getDescription() const override {
    return "OPAL-RTs AsyncIP example format";
  }
};

} // namespace node
} // namespace villas
