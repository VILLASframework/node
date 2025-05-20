/* JSON serializtion for RESERVE project.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/formats/json.hpp>

namespace villas {
namespace node {

class JsonReserveFormat : public JsonFormat {

protected:
  int packSample(json_t **j, const struct Sample *smp) override;
  int unpackSample(json_t *json_smp, struct Sample *smp) override;

public:
  using JsonFormat::JsonFormat;
};

} // namespace node
} // namespace villas
