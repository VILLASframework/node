/* UltraLight format for FISMEP project.
 *
 * Author: Iris Koester <ikoester@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class IotAgentUltraLightFormat : public Format {

protected:
  int sprint(char *buf, size_t len, size_t *wbytes,
             const struct Sample *const smps[], unsigned cnt) override;

  int sscan(const char *buf, size_t len, size_t *rbytes,
            struct Sample *const smps[], unsigned cnt) override;

public:
  using Format::Format;
};

} // namespace node
} // namespace villas
