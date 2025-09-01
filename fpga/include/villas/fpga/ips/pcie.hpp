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

#include <xilinx/xaxis_switch.h>

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class AxiPciExpressBridge : public Core {
public:
  friend class AxiPciExpressBridgeFactory;

  virtual bool init() override;

protected:
  virtual const char *getAxiInterfaceName() { return "M_AXI"; };
  static constexpr char pcieMemory[] = "BAR0";

  struct AxiBar {
    uintptr_t base;
    size_t size;
    uintptr_t translation;
  };

  struct PciBar {
    uintptr_t translation;
  };

  std::unordered_map<std::string, AxiBar> axiToPcieTranslations;
  std::unordered_map<std::string, PciBar> pcieToAxiTranslations;
};

class XDmaBridge : public AxiPciExpressBridge {
protected:
  virtual const char *getAxiInterfaceName() { return "M_AXI_B"; };
};

class AxiPciExpressBridgeFactory : CoreFactory {

public:
  virtual std::string getName() const { return "pcie"; }

  virtual std::string getDescription() const {
    return "Xilinx's AXI-PCIe Bridge";
  }

private:
  virtual Vlnv getCompatibleVlnv() const {
    return Vlnv("xilinx.com:ip:axi_pcie:");
  }

  // Create a concrete IP instance
  Core *make() const { return new AxiPciExpressBridge; };

protected:
  virtual void parse(Core &, json_t *) override;
};

class XDmaBridgeFactory : public AxiPciExpressBridgeFactory {

public:
  virtual std::string getDescription() const {
    return "Xilinx's XDMA IP configured as AXI-PCIe Bridge";
  }

private:
  virtual Vlnv getCompatibleVlnv() const { return Vlnv("xilinx.com:ip:xdma:"); }

  // Create a concrete IP instance
  Core *make() const { return new XDmaBridge; };
};

} // namespace ip
} // namespace fpga
} // namespace villas
