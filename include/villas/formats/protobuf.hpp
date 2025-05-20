/* Protobuf IO format.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class ProtobufFormat : public BinaryFormat {

public:
  using BinaryFormat::BinaryFormat;

  int sscan(const char *buf, size_t len, size_t *rbytes,
            struct Sample *const smps[], unsigned cnt) override;
  int sprint(char *buf, size_t len, size_t *wbytes,
             const struct Sample *const smps[], unsigned cnt) override;
};

} // namespace node
} // namespace villas
