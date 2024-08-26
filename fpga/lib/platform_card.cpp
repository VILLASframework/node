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

#include <jansson.h>
#include <string>
#include <villas/fpga/card_parser.hpp>
#include <villas/fpga/core.hpp>
#include <villas/fpga/node.hpp>
#include <villas/fpga/platform_card.hpp>
#include <villas/kernel/devices/device_ip_matcher.hpp>
#include <villas/kernel/devices/ip_device_reader.hpp>
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

  // Read devices from Devicetree
  const std::filesystem::path PLATFORM_DEVICES_DIRECTORY =
      std::filesystem::path("/sys/bus/platform/devices");
  auto dtr = IpDeviceReader(PLATFORM_DEVICES_DIRECTORY);
  std::vector<IpDevice> devices = dtr.devices;

  // Match devices and ips
  auto matcher = DeviceIpMatcher(devices, configuredIps);
  std::vector<std::pair<std::shared_ptr<ip::Core>, IpDevice>> device_ip_pair =
      matcher.match();

  // Bind device to platform driver
  auto driver =
      Driver(std::filesystem::path("/sys/bus/platform/drivers/vfio-platform"));
  for (auto pair : device_ip_pair) {
    auto device = pair.second;
    driver.attach(device);
  }

  // VFIO Setup
  for (auto pair : device_ip_pair) {
    auto device = pair.second;

    // Attach group to container
    const int iommu_group = device.iommu_group().value();

    auto vfio_group = vfioContainer->getOrAttachGroup(iommu_group);
    logger->debug("Device: {}, Iommu: {}", device.name(), iommu_group);

    // Open Vfio Device
    auto vfio_device = std::make_shared<kernel::vfio::Device>(
        device.name(), vfio_group->getFileDescriptor());

    // Attach device to group
    vfio_group->attachDevice(vfio_device);

    // Add as member
    this->vfio_devices.push_back(vfio_device);

    // Map vfio device to process
    const void *mapping = vfio_device->regionMap(0);
    if (mapping == MAP_FAILED) {
      logger->error("Failed to mmap() device");
    }
    logger->debug("memory mapped: {}", vfio_device->getName());

    // Create mappings from process space to vfio devices
    auto &mm = MemoryManager::get();
    size_t srcVertexId = mm.getProcessAddressSpace();
    const size_t mem_size = vfio_device->regionGetSize(0);
    size_t targetVertexId = mm.getOrCreateAddressSpace(vfio_device->getName());
    mm.createMapping(reinterpret_cast<uintptr_t>(mapping), 0, mem_size,
                     "process to vfio", srcVertexId, targetVertexId);
    logger->debug("create edge from process to {}", vfio_device->getName());

    auto ip = pair.first;
    // Connect vfio vertex to Reg vertex
    connect(vfio_device->getName(), ip);
  }
}

void PlatformCard::connect(std::string device_name,
                           std::shared_ptr<ip::Core> ip) {
  auto &mm = MemoryManager::get();
  const size_t ip_mem_size = 65536;

  size_t srcVertexId = mm.getOrCreateAddressSpace(device_name);

  std::string taget_address_space_name =
      ip->getInstanceName() + "/Reg"; //? TODO: Reg neded?
  size_t targetVertexId;
  targetVertexId = mm.getOrCreateAddressSpace(taget_address_space_name);

  mm.createMapping(0, 0, ip_mem_size, "vfio to ip", srcVertexId,
                   targetVertexId);

  logger->debug("Connect {} and {}", mm.getGraph().getVertex(srcVertexId)->name,
                mm.getGraph().getVertex(targetVertexId)->name);
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
  card->affinity = parser.affinity;
  card->doReset = parser.do_reset != 0;
  card->polling = (parser.polling != 0);

  json_t *json_ips = parser.json_ips;
  json_t *json_paths = parser.json_paths;
  json_error_t err;

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

      int ret;
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
