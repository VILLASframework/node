/* Zynq node
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023-2025 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Zynq : public Core {
public:
  friend class ZynqFactory;

  bool init() override;
};

class ZynqFactory : CoreFactory {

public:
  std::string getName() const override { return "Zynq"; }

  std::string getDescription() const override { return "Zynq based fpga"; }

private:
  Vlnv getCompatibleVlnv() const override {
    return Vlnv("xilinx.com:ip:zynq_ultra_ps_e:");
  }

  // Create a concrete IP instance
  Core *make() const override { return new Zynq; };

protected:
  void parse(Core &, json_t *) override;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
