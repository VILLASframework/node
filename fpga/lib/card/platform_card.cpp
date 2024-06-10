/* Platform based card
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/fpga/card/platform_card.hpp>

using namespace villas;
using namespace villas::fpga;

PlatformCard::PlatformCard(
    std::shared_ptr<kernel::vfio::Container> vfioContainer,
    std::vector<std::string> device_names) {
  this->vfioContainer = vfioContainer;
  this->logger = villas::logging.get("PlatformCard");

  // Create VFIO Group
  const int IOMMU_GROUP = 2; //TODO: find Group
  auto group = std::make_shared<kernel::vfio::Group>(IOMMU_GROUP, true);
  vfioContainer->attachGroup(group);

  // Open VFIO Devices
  for (std::string device_name : device_names) {
    auto vfioDevice = std::make_shared<kernel::vfio::Device>(
        device_name, group->getFileDescriptor());
    group->attachDevice(vfioDevice);
    this->devices.push_back(vfioDevice);
  }

  // Map all vfio devices in card to process
  std::map<std::shared_ptr<villas::kernel::vfio::Device>, const void *>
      mapped_memory;
  for (auto device : devices) {
    const void *mapping = device->regionMap(0);
    if (mapping == MAP_FAILED) {
      logger->error("Failed to mmap() device");
    }
    logger->debug("memory mapped: {}", device->getName());
    mapped_memory.insert({device, mapping});
  }

  // Create mappings from process space to vfio devices
  auto &mm = MemoryManager::get();
  size_t srcVertexId = mm.getProcessAddressSpace();
  for (auto pair : mapped_memory) {
    const size_t mem_size = pair.first->regionGetSize(0);
    size_t targetVertexId = mm.getOrCreateAddressSpace(pair.first->getName());
    mm.createMapping(reinterpret_cast<uintptr_t>(pair.second), 0, mem_size,
                     "process to vfio", srcVertexId, targetVertexId);
    logger->debug("create edge from process to {}", pair.first->getName());
  }
}

void PlatformCard::connectVFIOtoIPS() {
  // TODO: Connect based on memory addr
  auto &mm = MemoryManager::get();
  const size_t ip_mem_size = 65536;
  for (auto device : devices) {
    std::string addr;
    std::string name;

    std::istringstream iss(device->getName());
    std::getline(iss, addr, '.');
    std::getline(iss, name, '.');

    size_t srcVertexId = mm.getOrCreateAddressSpace(device->getName());
    size_t targetVertexId;

    if (name == "dma")
      targetVertexId = mm.getOrCreateAddressSpace("axi_dma_0/Reg");
    else if (name == "axis_switch")
      targetVertexId =
          mm.getOrCreateAddressSpace("axis_interconnect_0_xbar/Reg");

    mm.createMapping(0, 0, ip_mem_size, "vfio to ip", srcVertexId,
                     targetVertexId);
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

  auto space = mm.findAddressSpace("zynq_ultra_ps_e_0/HPC1_DDR_LOW");
  mm.createMapping(iovaAddr, 0, block->getSize(), "VFIO-D2H", space,
                   addrSpaceId);

  // Remember that this block has already been mapped for later
  memoryBlocksMapped.insert({addrSpaceId, block});

  return true;
}
