/* Driver for wrapper around standard Xilinx Aurora (xilinx.com:ip:aurora_8b10b)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class AuroraXilinx : public Node {
public:
  static constexpr const char *masterPort = "USER_DATA_M_AXI_RX";
  static constexpr const char *slavePort = "USER_DATA_S_AXI_TX";

  const StreamVertex &getDefaultSlavePort() const {
    return getSlavePort(slavePort);
  }

  const StreamVertex &getDefaultMasterPort() const {
    return getMasterPort(masterPort);
  }
};

} // namespace ip
} // namespace fpga
} // namespace villas
