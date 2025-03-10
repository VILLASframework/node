/* Zynq node
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
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

  virtual bool init() override;
};

class ZynqFactory : CoreFactory {

public:
  virtual std::string getName() const { return "Zynq"; }

  virtual std::string getDescription() const {
    return "Zynq based fpga";
  }

private:
  virtual Vlnv getCompatibleVlnv() const {
    return Vlnv("xilinx.com:ip:zynq_ultra_ps_e:");
  }

  // Create a concrete IP instance
  Core *make() const { return new Zynq; };

protected:
  virtual void parse(Core &, json_t *) override;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
