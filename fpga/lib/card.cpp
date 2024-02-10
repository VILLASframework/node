/* FPGA card
 *
 * This class represents a FPGA device.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/fpga/card.hpp>

using namespace villas;
using namespace villas::fpga;

Card::~Card()
{
  for (auto ip = ips.rbegin(); ip != ips.rend(); ++ip){
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

std::shared_ptr<ip::Core> Card::lookupIp(const std::string &name) const
{
        for(auto &ip : ips) {
                if(*ip == name) {
                        return ip;
                }
        }

        return nullptr;
}

std::shared_ptr<ip::Core> Card::lookupIp(const Vlnv &vlnv) const
{
        for(auto &ip : ips) {
                if(*ip == vlnv) {
                        return ip;
                }
        }

        return nullptr;
}

std::shared_ptr<ip::Core> Card::lookupIp(const ip::IpIdentifier &id) const
{
	for (auto &ip : ips) {
		if (*ip == id) {
			return ip;
		}
	}

	return nullptr;
}

bool Card::unmapMemoryBlock(const MemoryBlock& block)
{
	if (memoryBlocksMapped.find(block.getAddrSpaceId()) == memoryBlocksMapped.end()) {
		throw std::runtime_error("Block " + std::to_string(block.getAddrSpaceId()) + " is not mapped but was requested to be unmapped.");
	}

	auto &mm = MemoryManager::get();

	auto translation = mm.getTranslation(addrSpaceIdDeviceToHost, block.getAddrSpaceId());

	const uintptr_t iova = translation.getLocalAddr(0);
	const size_t size = translation.getSize();

	logger->debug("Unmap block {} at IOVA {:#x} of size {:#x}",
			block.getAddrSpaceId(), iova, size);
	vfioContainer->memoryUnmap(iova, size);

	memoryBlocksMapped.erase(block.getAddrSpaceId());

	return true;
}


bool Card::mapMemoryBlock(const std::shared_ptr<MemoryBlock> block)
{
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
	uintptr_t iovaAddr = vfioContainer->memoryMap(processBaseAddr,
	                                              UINTPTR_MAX,
	                                              block->getSize());

	if (iovaAddr == UINTPTR_MAX) {
		logger->error("Cannot map memory at {:#x} of size {:#x}",
		              processBaseAddr, block->getSize());
		return false;
	}

	mm.createMapping(iovaAddr, 0, block->getSize(),
	                 "VFIO-D2H",
	                 this->addrSpaceIdDeviceToHost,
	                 addrSpaceId);

	// Remember that this block has already been mapped for later
	memoryBlocksMapped.insert({addrSpaceId, block});

        return true;
}
