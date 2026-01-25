/* AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <unordered_map>

#include <xilinx/xaxis_switch.h>

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class AxiStreamSwitch : public Node {
public:
  friend class AxiStreamSwitchFactory;

  bool init() override;

  bool connectInternal(const std::string &slavePort,
                       const std::string &masterPort) override;

  void printConfig() const;

private:
  int portNameToNum(const std::string &portName);

  static constexpr const char *PORT_DISABLED = "DISABLED";

  static constexpr char registerMemory[] = "Reg";

  std::list<MemoryBlockName> getMemoryBlocks() const override {
    return {registerMemory};
  }

  XAxis_Switch xSwitch;
  XAxis_Switch_Config xConfig;

  std::unordered_map<std::string, std::string> portMapping;
};

class AxiStreamSwitchFactory : NodeFactory {

public:
  std::string getName() const override { return "switch"; }

  std::string getDescription() const override {
    return "Xilinx's AXI4-Stream switch";
  }

private:
  Vlnv getCompatibleVlnv() const override {
    return Vlnv("xilinx.com:ip:axis_switch:");
  }

  // Create a concrete IP instance
  Core *make() const override { return new AxiStreamSwitch; };

protected:
  void parse(Core &, json_t *) override;
};

} // namespace ip
} // namespace fpga
} // namespace villas
