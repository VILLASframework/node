/* FPGA pciecard.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fmt/ostream.h>
#include <memory>
#include <string>
#include <utility>
#include <villas/exceptions.hpp>
#include <villas/fpga/core.hpp>
#include <villas/fpga/node.hpp>
#include <villas/fpga/pcie_card.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/pci.hpp>
#include <villas/kernel/vfio_container.hpp>
#include <villas/memory.hpp>

using namespace villas;
using namespace villas::fpga;

// Instantiate factory to register
static PCIeCardFactory PCIeCardFactoryInstance;

static const kernel::pci::Device
    defaultFilter((kernel::pci::Id(FPGA_PCI_VID_XILINX, FPGA_PCI_PID_VFPGA)));

std::shared_ptr<PCIeCard>
PCIeCardFactory::make(json_t *json_card, std::string card_name,
                      std::shared_ptr<kernel::vfio::Container> vc,
                      const std::filesystem::path &searchPath) {
  auto logger = getStaticLogger();

  // make sure the vfio container has the required modules
  kernel::loadModule("vfio_pcie");
  kernel::loadModule("vfio_iommu_type1");

  json_t *json_ips = nullptr;
  json_t *json_paths = nullptr;
  const char *pci_slot = nullptr;
  const char *pci_id = nullptr;
  int do_reset = 0;
  int affinity = 0;
  int polling = 0;

  json_error_t err;
  int ret = json_unpack_ex(
      json_card, &err, 0, "{ s: o, s?: i, s?: b, s?: s, s?: s, s?: b, s?: o }",
      "ips", &json_ips, "affinity", &affinity, "do_reset", &do_reset, "slot",
      &pci_slot, "id", &pci_id, "polling", &polling, "paths", &json_paths);

  if (ret != 0)
    throw ConfigError(json_card, err, "", "Failed to parse card");

  auto card = std::shared_ptr<PCIeCard>(make());

  // Populate generic properties
  card->name = std::string(card_name);
  card->vfioContainer = vc;
  card->affinity = affinity;
  card->doReset = do_reset != 0;
  card->polling = (polling != 0);

  kernel::pci::Device filter = defaultFilter;

  if (pci_id)
    filter.id = kernel::pci::Id(pci_id);
  if (pci_slot)
    filter.slot = kernel::pci::Slot(pci_slot);

  // Search for FPGA card
  card->pdev = kernel::pci::DeviceList::getInstance()->lookupDevice(filter);
  if (!card->pdev) {
    logger->warn("Failed to find PCI device");
    return nullptr;
  }

  if (not card->init()) {
    logger->warn("Cannot start FPGA card {}", card_name);
    return nullptr;
  }

  // Load IPs from a separate json file
  if (!json_is_string(json_ips)) {
    logger->debug("FPGA IP cores config item is not a string.");
    throw ConfigError(json_ips, "node-config-fpga-ips",
                      "FPGA IP cores config item is not a string.");
  }
  if (!searchPath.empty()) {
    std::filesystem::path json_ips_path =
        searchPath / json_string_value(json_ips);
    logger->debug("searching for FPGA IP cors config at {}",
                  json_ips_path.string());
    json_ips = json_load_file(json_ips_path.c_str(), 0, nullptr);
  } else {
    json_ips = json_load_file(json_string_value(json_ips), 0, nullptr);
  }
  if (json_ips == nullptr) {
    json_ips = json_load_file(json_string_value(json_ips), 0, nullptr);
    logger->debug("searching for FPGA IP cors config at {}",
                  json_string_value(json_ips));
    if (json_ips == nullptr) {
      throw ConfigError(json_ips, "node-config-fpga-ips",
                        "Failed to find FPGA IP cores config");
    }
  }

  if (not json_is_object(json_ips))
    throw ConfigError(json_ips, "node-config-fpga-ips",
                      "FPGA IP core list must be an object!");

  ip::CoreFactory::make(card.get(), json_ips);
  if (card->ips.empty())
    throw ConfigError(json_ips, "node-config-fpga-ips",
                      "Cannot initialize IPs of FPGA card {}", card_name);

  if (not card->check())
    throw RuntimeError("Checking of FPGA card {} failed", card_name);

  // Additional static paths for AXI-Steram switch
  if (json_paths != nullptr) {
    if (not json_is_array(json_paths))
      throw ConfigError(json_paths, err, "",
                        "Switch path configuration must be an array");

    size_t i;
    json_t *json_path;
    json_array_foreach(json_paths, i, json_path) {
      const char *from, *to;
      int reverse = 0;

      ret = json_unpack_ex(json_path, &err, 0, "{ s: s, s: s, s?: b }", "from",
                           &from, "to", &to, "reverse", &reverse);
      if (ret != 0)
        throw ConfigError(json_path, err, "",
                          "Cannot parse switch path config");

      auto masterIpCore = card->lookupIp(from);
      if (!masterIpCore)
        throw ConfigError(json_path, "", "Unknown IP {}", from);

      auto slaveIpCore = card->lookupIp(to);
      if (!slaveIpCore)
        throw ConfigError(json_path, "", "Unknown IP {}", to);

      auto masterIpNode = std::dynamic_pointer_cast<ip::Node>(masterIpCore);
      if (!masterIpNode)
        throw ConfigError(json_path, "", "IP {} is not a streaming node", from);

      auto slaveIpNode = std::dynamic_pointer_cast<ip::Node>(slaveIpCore);
      if (!slaveIpNode)
        throw ConfigError(json_path, "", "IP {} is not a streaming node", to);

      if (not masterIpNode->connect(*slaveIpNode, reverse != 0))
        throw ConfigError(json_path, "", "Failed to connect node {} to {}",
                          from, to);
    }
  }
  // Deallocate JSON config
  json_decref(json_ips);
  return card;
}

PCIeCard::~PCIeCard() {}

bool PCIeCard::init() {
  logger = getLogger();

  logger->info("Initializing FPGA card {}", name);

  // Attach PCIe card to VFIO container
  vfioDevice = vfioContainer->attachDevice(*pdev);

  // Enable memory access and PCI bus mastering for DMA
  if (not vfioDevice->pciEnable()) {
    logger->error("Failed to enable PCI device");
    return false;
  }

  // Reset system?
  if (doReset) {
    // Reset / detect PCI device
    if (not vfioDevice->pciHotReset()) {
      logger->error("Failed to reset PCI device");
      return false;
    }

    if (not reset()) {
      logger->error("Failed to reset FGPA card");
      return false;
    }
  }

  return true;
}
