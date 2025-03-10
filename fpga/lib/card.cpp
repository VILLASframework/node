/* FPGA card
 *
 * This class represents a FPGA device.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/node.hpp>

using namespace villas;
using namespace villas::fpga;

Card::~Card() {
  for (auto ip = ips.rbegin(); ip != ips.rend(); ++ip) {
    (*ip)->stop();
  }
  // Ensure IP destructors are called before memory is unmapped
  ips.clear();

  auto &mm = MemoryManager::get();

  // Unmap all memory blocks
  for (auto &mappedMemoryBlock : memoryBlocksMapped) {
    auto translation =
        mm.getTranslation(addrSpaceIdDeviceToHost, mappedMemoryBlock.first);

    const uintptr_t iova = translation.getLocalAddr(0);
    const size_t size = translation.getSize();

    logger->debug("Unmap block {} at IOVA {:#x} of size {:#x}",
                  mappedMemoryBlock.first, iova, size);
    vfioContainer->memoryUnmap(iova, size);
  }
}

std::shared_ptr<ip::Core> Card::lookupIp(const std::string &name) const {
  for (auto &ip : ips) {
    if (*ip == name) {
      return ip;
    }
  }

  return nullptr;
}

std::shared_ptr<ip::Core> Card::lookupIp(const Vlnv &vlnv) const {
  for (auto &ip : ips) {
    if (*ip == vlnv) {
      return ip;
    }
  }

  return nullptr;
}

std::shared_ptr<ip::Core> Card::lookupIp(const ip::IpIdentifier &id) const {
  for (auto &ip : ips) {
    if (*ip == id) {
      return ip;
    }
  }

  return nullptr;
}

bool Card::unmapMemoryBlock(const MemoryBlock &block) {
  if (memoryBlocksMapped.find(block.getAddrSpaceId()) ==
      memoryBlocksMapped.end()) {
   throw std::runtime_error(
        "Block " + std::to_string(block.getAddrSpaceId()) +
        " is not mapped but was requested to be unmapped.");
  }

  auto &mm = MemoryManager::get();

  auto translation =
      mm.getTranslation(addrSpaceIdDeviceToHost, block.getAddrSpaceId());

  const uintptr_t iova = translation.getLocalAddr(0);
  const size_t size = translation.getSize();

  logger->debug("Unmap block {} at IOVA {:#x} of size {:#x}",
                block.getAddrSpaceId(), iova, size);
  vfioContainer->memoryUnmap(iova, size);

  memoryBlocksMapped.erase(block.getAddrSpaceId());

  return true;
}

bool Card::mapMemoryBlock(const std::shared_ptr<MemoryBlock> block) {
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
    logger->debug("Create VFIO mapping for {}", addrSpaceId);

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

  // Remember that this block has already been mapped for later
  memoryBlocksMapped.insert({addrSpaceId, block});

  return true;
}

void CardFactory::loadIps(std::shared_ptr<Card> card, json_t *json_ips,
                          const std::filesystem::path &searchPath) {
  auto logger = getStaticLogger();

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
                      "Cannot initialize IPs of FPGA card {}", card->name);
};

void CardFactory::loadSwitch(std::shared_ptr<Card> card, json_t *json_paths) {
  // Additional static paths for AXI-Stream switch
  json_error_t err;
  if (json_paths != nullptr) {
    if (not json_is_array(json_paths))
      throw ConfigError(json_paths, err, "",
                        "Switch path configuration must be an array");

    size_t i;
    json_t *json_path;
    json_array_foreach(json_paths, i, json_path) {
      const char *from, *to;
      int reverse = 0;

      int ret = json_unpack_ex(json_path, &err, 0, "{ s: s, s: s, s?: b }",
                               "from", &from, "to", &to, "reverse", &reverse);
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
}
