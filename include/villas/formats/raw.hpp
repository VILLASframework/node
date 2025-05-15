/* RAW IO format.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdlib>
#include <sstream>

#include <villas/format.hpp>

// float128 is currently not yet supported as htole128() functions a missing
#if 0 && defined(__GNUC__) && defined(__linux__)
#define HAS_128BIT
#endif

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class RawFormat : public BinaryFormat {

public:
  enum Endianess { BIG, LITTLE };

protected:
  enum Endianess endianess;
  int bits;
  bool fake;

public:
  RawFormat(int fl, int b = 32, enum Endianess e = Endianess::LITTLE)
      : BinaryFormat(fl), endianess(e), bits(b), fake(false) {
    if (fake)
      flags |= (int)SampleFlags::HAS_SEQUENCE | (int)SampleFlags::HAS_TS_ORIGIN;
  }

  int sscan(const char *buf, size_t len, size_t *rbytes,
            struct Sample *const smps[], unsigned cnt) override;
  int sprint(char *buf, size_t len, size_t *wbytes,
             const struct Sample *const smps[], unsigned cnt) override;

  void parse(json_t *json) override;
};

class GtnetRawFormat : public RawFormat {

public:
  GtnetRawFormat(int fl) : RawFormat(fl, 32, Endianess::BIG) {}
};

} // namespace node
} // namespace villas
