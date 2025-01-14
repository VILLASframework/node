/* The VILLASframework sample format.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdio>

#include <villas/format.hpp>

namespace villas {
namespace node {

class ValueFormat : public Format {

public:
  using Format::Format;

  virtual int sscan(const char *buf, size_t len, size_t *rbytes,
                    struct Sample *const smps[], unsigned cnt);
  virtual int sprint(char *buf, size_t len, size_t *wbytes,
                     const struct Sample *const smps[], unsigned cnt);
};

} // namespace node
} // namespace villas
