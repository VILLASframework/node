/** Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <map>
#include <list>
#include <memory>
#include <jansson.h>

#include <villas/log.hpp>
#include <villas/colors.hpp>
#include <villas/memory.hpp>
#include <villas/plugin.hpp>

#include <villas/fpga/vlnv.hpp>

namespace villas {
namespace fpga {

// forward declaration
class PCIeCard;

namespace ip {

// forward declarations
class Core;
class CoreFactory;
class InterruptController;


class IpIdentifier {
public:

	IpIdentifier(Vlnv vlnv = Vlnv::getWildcard(), std::string name = "") :
	    vlnv(vlnv), name(name) {}

	IpIdentifier(std::string vlnvString, std::string name = "") :
	    vlnv(vlnvString), name(name) {}

	const std::string&
	getName() const
	{ return name; }

	const Vlnv&
	getVlnv() const
	{ return vlnv; }

	friend std::ostream&
	operator<< (std::ostream &stream, const IpIdentifier &id)
	{ return stream << id.name << " vlnv=" << id.vlnv; }

	bool
	operator==(const IpIdentifier &otherId) const {
		const bool vlnvWildcard = otherId.getVlnv() == Vlnv::getWildcard();
		const bool nameWildcard = this->getName().empty() or otherId.getName().empty();

		const bool vlnvMatch = vlnvWildcard or this->getVlnv() == otherId.getVlnv();
		const bool nameMatch = nameWildcard or this->getName() == otherId.getName();

		return vlnvMatch and nameMatch;
	}

	bool
	operator!=(const IpIdentifier &otherId) const
	{ return !(*this == otherId); }

private:
	Vlnv vlnv;
	std::string name;
};


class Core {
	friend CoreFactory;

public:
	Core() : card(nullptr) {}
	virtual ~Core() = default;

	using Ptr = std::shared_ptr<Core>;
	using List = std::list<Core::Ptr>;

public:
	/* Generic management interface for IPs */

	/// Runtime setup of IP, should access and initialize hardware
	virtual bool init()
	{ return true; }

	/// Runtime check of IP, should verify basic functionality
	virtual bool check() { return true; }

	/// Generic disabling of IP, meaning may depend on IP
	virtual bool stop()  { return true; }

	/// Reset the IP, it should behave like freshly initialized afterwards
	virtual bool reset() { return true; }

	/// Print some debug information about the IP
	virtual void dump();

protected:
	/// Key-type for accessing maps addressTranslations and slaveAddressSpaces
	using MemoryBlockName = std::string;

	/// Each IP can declare via this function which memory blocks it requires
	virtual std::list<MemoryBlockName>
	getMemoryBlocks() const
	{ return {}; }

public:
	const std::string&
	getInstanceName() const
	{ return id.getName(); }

	/* Operators */

	bool
	operator==(const Vlnv &otherVlnv) const
	{ return id.getVlnv() == otherVlnv; }

	bool
	operator!=(const Vlnv &otherVlnv) const
	{ return id.getVlnv() != otherVlnv; }

	bool
	operator==(const IpIdentifier &otherId) const
	{ return this->id == otherId; }

	bool
	operator!=(const IpIdentifier &otherId) const
	{ return this->id != otherId; }

	bool
	operator==(const std::string &otherName) const
	{ return getInstanceName() == otherName; }

	bool
	operator!=(const std::string &otherName) const
	{ return getInstanceName() != otherName; }

	bool
	operator==(const Core &otherIp) const
	{ return this->id == otherIp.id; }

	bool
	operator!=(const Core &otherIp) const
	{ return this->id != otherIp.id; }

	friend std::ostream&
	operator<< (std::ostream &stream, const Core &ip)
	{ return stream << ip.id; }

protected:
	uintptr_t
	getBaseAddr(const MemoryBlockName &block) const
	{ return getLocalAddr(block, 0); }

	uintptr_t
	getLocalAddr(const MemoryBlockName &block, uintptr_t address) const;

	MemoryManager::AddressSpaceId
	getAddressSpaceId(const MemoryBlockName &block) const
	{ return slaveAddressSpaces.at(block); }

	InterruptController*
	getInterruptController(const std::string &interruptName) const;

	MemoryManager::AddressSpaceId
	getMasterAddrSpaceByInterface(const std::string &masterInterfaceName) const
	{ return busMasterInterfaces.at(masterInterfaceName); }

	template<typename T>
	T readMemory(const std::string &block, uintptr_t address) const
	{ return *(reinterpret_cast<T*>(getLocalAddr(block, address))); }
	
	template<typename T>
	void writeMemory(const std::string &block, uintptr_t address, T value)
	{ T* ptr = reinterpret_cast<T*>(getLocalAddr(block, address)); *ptr = value; }

protected:
	struct IrqPort {
		int num;
		InterruptController* irqController;
		std::string description;
	};

	/// Specialized logger instance with the IPs name set as category
	Logger logger;

	/// FPGA card this IP is instantiated on (populated by FpgaIpFactory)
	PCIeCard* card;

	/// Identifier of this IP with its instance name and VLNV
	IpIdentifier id;

	/// All interrupts of this IP with their associated interrupt controller
	std::map<std::string, IrqPort> irqs;

	/// Cached translations from the process address space to each memory block
	std::map<MemoryBlockName, MemoryTranslation> addressTranslations;

	/// Lookup for IP's slave address spaces (= memory blocks)
	std::map<MemoryBlockName, MemoryManager::AddressSpaceId> slaveAddressSpaces;

	/// AXI bus master interfaces to access memory somewhere
	std::map<std::string, MemoryManager::AddressSpaceId> busMasterInterfaces;
};



class CoreFactory : public plugin::Plugin {
public:
	using plugin::Plugin::Plugin;

	/// Returns a running and checked FPGA IP
	static Core::List
	make(PCIeCard* card, json_t *json_ips);

protected:
	Logger
	getLogger() const
	{ return villas::logging.get(getName()); }

private:
	/// Create a concrete IP instance
	virtual Core* create() = 0;

	/// Configure IP instance from JSON config
	virtual bool configureJson(Core& /* ip */, json_t* /* json */)
	{ return true; }

	virtual Vlnv getCompatibleVlnv() const = 0;

protected:
	static Logger
	getStaticLogger() { return villas::logging.get("CoreFactory"); }

private:
	static CoreFactory*
	lookup(const Vlnv &vlnv);
};

/** @} */

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
