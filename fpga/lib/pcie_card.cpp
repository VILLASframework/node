/* FPGA pciecard.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <string>
#include <utility>

#include <fmt/ostream.h>

#include <villas/exceptions.hpp>
#include <villas/fpga/core.hpp>
#include <villas/fpga/devices/pci_device.hpp>
#include <villas/fpga/memory.hpp>
#include <villas/fpga/pcie_card.hpp>
#include <villas/fpga/vfio/container.hpp>
#include <villas/json.hpp>
#include <villas/kernel/kernel.hpp>

using namespace villas;
using namespace villas::fpga;

// Instantiate factory to register
static PCIeCardFactory PCIeCardFactoryInstance;

static const fpga::devices::PciDevice
    defaultFilter((fpga::devices::Id(FPGA_PCI_VID_XILINX, FPGA_PCI_PID_VFPGA)));

std::shared_ptr<PCIeCard>
PCIeCardFactory::make(json_t *json_card, std::string card_name,
                      std::shared_ptr<vfio::Container> vc,
                      const fs::path &searchPath) {
  auto logger = getStaticLogger();

  // make sure the vfio container has the required modules
  kernel::loadModule("vfio_pci");
  kernel::loadModule("vfio_iommu_type1");

  json_t *json_ips = nullptr;
  json_t *json_paths = nullptr;
  const char *pci_slot = nullptr;
  const char *pci_id = nullptr;
  int do_reset = 0;
  int affinity = 0;
  int polling = 0;
  json_t *ignored_ips_array = nullptr;

  json_error_t err;
  int ret = json_unpack_ex(
      json_card, &err, 0,
      "{ s: o, s?: i, s?: b, s?: s, s?: s, s?: b, s?: o, s?: o}", "ips",
      &json_ips, "affinity", &affinity, "do_reset", &do_reset, "slot",
      &pci_slot, "id", &pci_id, "polling", &polling, "paths", &json_paths,
      "ignore_ips", &ignored_ips_array);

  if (ret != 0)
    throw JsonError(""_json_pointer, "Failed to parse card: {}", err.text);

  auto card = std::shared_ptr<PCIeCard>(make());

  // Populate generic properties
  card->name = std::string(card_name);
  card->vfioContainer = vc;
  card->affinity = affinity;
  card->doReset = do_reset != 0;
  card->polling = (polling != 0);

  // Parse ignored ip names to list
  size_t index;
  json_t *value;
  json_array_foreach (ignored_ips_array, index, value) {
    card->ignored_ip_names.push_back(json_string_value(value));
  }

  devices::PciDevice filter = defaultFilter;

  if (pci_id)
    filter.id = devices::Id(pci_id);
  if (pci_slot)
    filter.slot = devices::Slot(pci_slot);

  // Search for FPGA card
  card->pdev = devices::PciDeviceList::getInstance()->lookupDevice(filter);
  if (!card->pdev) {
    logger->warn("Failed to find PCI device");
    return nullptr;
  }

  if (not card->init()) {
    logger->warn("Cannot start FPGA card {}", card_name);
    return nullptr;
  }

  JsonError::context("/ips"_json_pointer, [&] {
    return CardFactory::loadIps(card, json_ips, searchPath);
  });

  if (not card->check())
    throw RuntimeError("Checking of FPGA card {} failed", card_name);

  JsonError::context("/paths"_json_pointer,
                     [&] { return CardFactory::loadSwitch(card, json_paths); });

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
