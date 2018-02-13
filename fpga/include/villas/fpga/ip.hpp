/** Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#ifndef VILLAS_IP_HPP
#define VILLAS_IP_HPP

#include "fpga/vlnv.hpp"
#include "plugin.hpp"
#include "log.hpp"

#include <map>
#include <list>
#include <memory>

#include <jansson.h>

#include "memory_manager.hpp"

namespace villas {
namespace fpga {

// forward declaration
class PCIeCard;

namespace ip {

// forward declarations
class IpCore;
class IpCoreFactory;
class InterruptController;

using IpCoreList = std::list<std::unique_ptr<IpCore>>;


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
	operator<< (std::ostream& stream, const IpIdentifier& id)
	{ return stream << TXT_BOLD(id.name) << " vlnv=" << id.vlnv; }

private:
	Vlnv vlnv;
	std::string name;
};


class IpCore {
public:

	friend IpCoreFactory;

	IpCore() : card(nullptr) {}
	virtual ~IpCore() {}

	// IPs can implement this interface
	virtual bool check() { return true; }
	virtual bool init() { return true; }
	virtual bool stop()  { return true; }
	virtual bool reset() { return true; }
	virtual void dump();

	bool
	operator== (const IpIdentifier& otherId) {
		const bool vlnvMatch = id.getVlnv() == otherId.getVlnv();
		const bool nameWildcard = id.getName().empty() or otherId.getName().empty();

		return vlnvMatch and (nameWildcard or id.getName() == otherId.getName());
	}

	bool
	operator!= (const IpIdentifier& otherId) {
		return !(*this == otherId);
	}

	bool
	operator== (const Vlnv& otherVlnv)
	{ return id.getVlnv() == otherVlnv; }

	bool
	operator== (const std::string& otherName)
	{ return id.getName() == otherName; }


	friend std::ostream&
	operator<< (std::ostream& stream, const IpCore& ip)
	{ return stream << ip.id; }

	const std::string&
	getInstanceName()
	{ return id.getName(); }

protected:

	uintptr_t
	getBaseAddr(const std::string& block) const;

	uintptr_t
	getLocalAddr(const std::string& block, uintptr_t address) const;

	SpdLogger
	getLogger() { return loggerGetOrCreate(id.getName()); }

	struct IrqPort {
		int num;
		InterruptController* irqController;
		std::string description;
	};

	InterruptController*
	getInterruptController(const std::string& interruptName);

	/// Each IP can declare via this function which memory blocks it requires
	virtual std::list<std::string>
	getMemoryBlocks() const
	{ return {}; }

protected:
	/// FPGA card this IP is instantiated on (populated by FpgaIpFactory)
	PCIeCard* card;

	/// Identifier of this IP with its instance name and VLNV
	IpIdentifier id;

	/// All interrupts of this IP with their associated interrupt controller
	std::map<std::string, IrqPort> irqs;

	/// Cached translations from the process address space to each memory block
	std::map<std::string, MemoryTranslation> addressTranslations;
};




class IpCoreFactory : public Plugin {
public:
	IpCoreFactory(std::string concreteName) :
	    Plugin(Plugin::Type::FpgaIp, std::string("IpCore - ") + concreteName)
	{}

	/// Returns a running and checked FPGA IP
	static IpCoreList
	make(PCIeCard* card, json_t *json_ips);

protected:
	SpdLogger
	getLogger() { return loggerGetOrCreate(getName()); }

private:
	/// Create a concrete IP instance
	virtual IpCore* create() = 0;

	/// Configure IP instance from JSON config
	virtual bool configureJson(IpCore& /* ip */, json_t* /* json */)
	{ return true; }

	virtual Vlnv getCompatibleVlnv() const = 0;
	virtual std::string getName() const = 0;
	virtual std::string getDescription() const = 0;

protected:
	static SpdLogger
	getStaticLogger() { return loggerGetOrCreate("IpCoreFactory"); }

private:
	static IpCoreFactory*
	lookup(const Vlnv& vlnv);
};

/** @} */

} // namespace ip
} // namespace fpga
} // namespace villas

#endif // VILLAS_IP_HPP
