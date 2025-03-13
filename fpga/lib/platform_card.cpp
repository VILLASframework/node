/* Platform based card
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 *
 * SPDX-FileCopyrightText: 2023-2025 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>
#include <string>

#include <villas/fpga/card_parser.hpp>
#include <villas/fpga/platform_card.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/memory_manager.hpp>

using namespace villas;
using namespace villas::fpga;
using namespace villas::kernel;
using namespace villas::kernel::devices;

PlatformCard::PlatformCard(
    std::shared_ptr<kernel::vfio::Container> vfioContainer) {
  this->vfioContainer = vfioContainer;
  this->logger = villas::Log::get("PlatformCard");
}

void PlatformCard::connectVFIOtoIps(
    std::list<std::shared_ptr<ip::Core>> configuredIps) {
  for (auto ip : configuredIps) {
    try {
      auto core_connection = CoreConnection::from(ip, this->vfioContainer);
      core_connection.add_to_memorygraph();
      this->core_connections.push_back(core_connection);
    } catch (const std::runtime_error &e) {
      logger->debug("Could not establish connection for {} .",
                    ip->getInstanceName());
    }
  }
}

bool PlatformCard::mapMemoryBlock(const std::shared_ptr<MemoryBlock> block) {
  if (not vfioContainer->isIommuEnabled()) {
    logger->warn("VFIO mapping not supported without IOMMU");
    return false;
  }

  auto &mm = MemoryManager::get();
  const auto &addrSpaceId = block->getAddrSpaceId();

  if (memoryBlocksMapped.find(addrSpaceId) != memoryBlocksMapped.end())
    // Block already mapped
    return true;
  else
    logger->debug("Create VFIO-Platform mapping for {}", addrSpaceId);

  auto translationFromProcess = mm.getTranslationFromProcess(addrSpaceId);
  uintptr_t processBaseAddr = translationFromProcess.getLocalAddr(0);
  uintptr_t iovaAddr =
      vfioContainer->memoryMap(processBaseAddr, UINTPTR_MAX, block->getSize());

  if (iovaAddr == UINTPTR_MAX) {
    logger->error("Cannot map memory at {:#x} of size {:#x}", processBaseAddr,
                  block->getSize());
    return false;
  }

  mm.createMapping(iovaAddr, 0, block->getSize(), "VFIO-D2H",
                   this->addrSpaceIdDeviceToHost, addrSpaceId);

  // TODO: Fix with multiple addr. space in zynq
  auto space = mm.findAddressSpace(
      "zynq_zynq_ultra_ps_e_0:M_AXI_HPM0_FPD"); // TODO: Remove hardcoded name
  mm.createMapping(iovaAddr, 0, block->getSize(), "VFIO-D2H", space,
                   addrSpaceId);

  // Remember that this block has already been mapped for later
  memoryBlocksMapped.insert({addrSpaceId, block});

  return true;
}

std::shared_ptr<PlatformCard>
PlatformCardFactory::make(json_t *json_card, std::string card_name,
                          std::shared_ptr<kernel::vfio::Container> vc,
                          const std::filesystem::path &searchPath) {
  auto logger = villas::Log::get("PlatformCardFactory");

  // make sure the vfio container has the required modules
  kernel::loadModule("vfio_platform");

  CardParser parser(json_card);

  auto card = std::make_shared<fpga::PlatformCard>(vc);
  card->name = std::string(card_name);
  card->doReset = parser.do_reset != 0;
  card->polling = parser.polling != 0;
  card->ignored_ip_names = parser.ignored_ip_names;

  CardFactory::loadIps(card, parser.json_ips, searchPath);
  CardFactory::loadSwitch(card, parser.json_paths);

  // Deallocate JSON config
  json_decref(parser.json_ips);
  return card;
}
