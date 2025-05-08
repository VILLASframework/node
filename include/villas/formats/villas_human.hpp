/* The VILLASframework sample format.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdio>

#include <villas/formats/line.hpp>

namespace villas {
namespace node {

class VILLASHumanFormat : public LineFormat {

protected:
  size_t sprintLine(char *buf, size_t len, const struct Sample *smp) override;
  size_t sscanLine(const char *buf, size_t len, struct Sample *smp) override;

public:
  using LineFormat::LineFormat;

  void header(FILE *f, const SignalList::Ptr sigs) override;
};

} // namespace node
} // namespace villas
