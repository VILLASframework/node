/* FPGA pciecard
 *
 * This class represents a FPGA device.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <jansson.h>
#include <list>
#include <set>
#include <string>

#include <villas/memory.hpp>
#include <villas/plugin.hpp>

#include <villas/kernel/devices/pci_device.hpp>
#include <villas/kernel/vfio_container.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/config.h>
#include <villas/fpga/core.hpp>

namespace villas {
namespace fpga {

// Forward declarations
struct vfio_container;
class PCIeCardFactory;

class PCIeCard : public Card {
public:
  ~PCIeCard();

  bool init();

  bool stop() { return true; }

  bool check() { return true; }

  bool reset() {
    // TODO: Try via sysfs?
    // echo 1 > /sys/bus/pci/devices/0000\:88\:00.0/reset
    return true;
  }

  void dump() {}

public:         // TODO: make this private
  bool doReset; // Reset VILLASfpga during startup?
  int affinity; // Affinity for MSI interrupts

  std::shared_ptr<kernel::pci::PciDevice> pdev; // PCI device handle

protected:
  Logger getLogger() const { return villas::Log::get(name); }
};

class PCIeCardFactory : public plugin::Plugin {
public:
  static std::shared_ptr<PCIeCard>
  make(json_t *json, std::string card_name,
       std::shared_ptr<kernel::vfio::Container> vc,
       const std::filesystem::path &searchPath);

  static PCIeCard *make() { return new PCIeCard(); }

  static Logger getStaticLogger() {
    return villas::Log::get("pcie:card:factory");
  }

  virtual std::string getName() const { return "pcie"; }

  virtual std::string getDescription() const {
    return "Xilinx PCIe FPGA cards";
  }

  virtual std::string getType() const { return "card"; }
};

} // namespace fpga
} // namespace villas
