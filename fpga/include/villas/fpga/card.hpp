/* FPGA card
 *
 * This class represents a FPGA device.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/core.hpp>
#include <villas/kernel/vfio_container.hpp>

namespace villas {
namespace fpga {

class Card {
public:
  bool polling;
  bool doReset;					// Reset VILLASfpga during startup?
	int affinity;					// Affinity for MSI interrupts
	std::string name;			// The name of the FPGA card
  std::shared_ptr<kernel::vfio::Container> vfioContainer;
  std::shared_ptr<kernel::vfio::Device> vfioDevice;

  // Slave address space ID to access the PCIe address space from the
  // FPGA
  MemoryManager::AddressSpaceId addrSpaceIdDeviceToHost;

  // Address space identifier of the master address space of this FPGA
  // card. This will be used for address resolution of all IPs on this
  // card.
  MemoryManager::AddressSpaceId addrSpaceIdHostToDevice;

  std::list<std::shared_ptr<ip::Core>> ips;
  std::list<std::string> ignored_ip_names;

  virtual ~Card();

  virtual void
  connectVFIOtoIps(std::list<std::shared_ptr<ip::Core>> configuredIps) = 0;

  virtual bool mapMemoryBlock(const std::shared_ptr<MemoryBlock> block);
  virtual bool unmapMemoryBlock(const MemoryBlock &block);

  std::shared_ptr<ip::Core> lookupIp(const std::string &name) const;
  std::shared_ptr<ip::Core> lookupIp(const Vlnv &vlnv) const;
  std::shared_ptr<ip::Core> lookupIp(const ip::IpIdentifier &id) const;

protected:
  // Keep a map of already mapped memory blocks
  std::map<MemoryManager::AddressSpaceId, std::shared_ptr<MemoryBlock>>
      memoryBlocksMapped;

  Logger logger;
};

} // namespace fpga
} // namespace villas
