/** FPGA pciecard
 *
 * This class represents a FPGA device.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

#include <list>
#include <set>
#include <string>
#include <jansson.h>

#include <villas/plugin.hpp>
#include <villas/memory.hpp>

#include <villas/kernel/pci.hpp>
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

	bool stop()
	{
		return true;
	}

	bool check()
	{
		return true;
	}

	bool reset()
	{
		// TODO: Try via sysfs?
		// echo 1 > /sys/bus/pci/devices/0000\:88\:00.0/reset
		return true;
	}

	void dump()
	{ }

	std::shared_ptr<ip::Core>
	lookupIp(const std::string &name) const;

	std::shared_ptr<ip::Core>
	lookupIp(const Vlnv &vlnv) const;

	std::shared_ptr<ip::Core>
	lookupIp(const ip::IpIdentifier &id) const;

	bool mapMemoryBlock(const MemoryBlock &block);
	bool unmapMemoryBlock(const MemoryBlock &block);

private:
	// Cache a set of already mapped memory blocks
	std::set<MemoryManager::AddressSpaceId> memoryBlocksMapped;

public:	// TODO: make this private
	std::list<std::shared_ptr<ip::Core>> ips;				// IPs located on this FPGA card

	bool doReset;					// Reset VILLASfpga during startup?
	int affinity;					// Affinity for MSI interrupts
	bool polling;					// Poll on interrupts?

	std::string name;				// The name of the FPGA card

	std::shared_ptr<kernel::pci::Device> pdev;	// PCI device handle

	// The VFIO container that this card is part of
	std::shared_ptr<kernel::vfio::Container> vfioContainer;

	// The VFIO device that represents this card
	std::shared_ptr<kernel::vfio::Device> vfioDevice;

	// Slave address space ID to access the PCIe address space from the FPGA
	MemoryManager::AddressSpaceId addrSpaceIdDeviceToHost;

	// Address space identifier of the master address space of this FPGA card.
	// This will be used for address resolution of all IPs on this card.
	MemoryManager::AddressSpaceId addrSpaceIdHostToDevice;

protected:
	Logger
	getLogger() const
	{
		return villas::logging.get(name);
	}

	Logger logger;
};

class PCIeCardFactory : public plugin::Plugin {
public:

	static
	std::list<std::shared_ptr<PCIeCard>> make(json_t *json, std::shared_ptr<kernel::pci::DeviceList> pci, std::shared_ptr<kernel::vfio::Container> vc);

	static
	PCIeCard* make()
	{
		return new PCIeCard();
	}

	static Logger
	getStaticLogger()
	{
		return villas::logging.get("pcie:card:factory");
	}

	virtual std::string
	getName() const
	{
		return "pcie";
	}

	virtual std::string
	getDescription() const
	{
		return "Xilinx PCIe FPGA cards";
	}

	virtual
	std::string getType() const
	{
		return "card";
	}
};

} /* namespace fpga */
} /* namespace villas */
