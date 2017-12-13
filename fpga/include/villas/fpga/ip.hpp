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

#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "common.h"

#include "log.h"
#include "utils.h"

#include "fpga/vlnv.hpp"

#include "plugin.hpp"
#include "card.hpp"

#include <jansson.h>


namespace villas {
namespace fpga {
namespace ip {


class IpCoreFactory;

class IpCore {
public:

	friend IpCoreFactory;

	IpCore() : card(nullptr), baseaddr(0), irq(-1), port(-1) {}
	virtual ~IpCore() {}

	// IPs can implement this interface
	virtual bool check() { return true; }
	virtual bool start() { return true; }
	virtual bool stop()  { return true; }
	virtual bool reset() { return true; }
	virtual void dump();

protected:
	uintptr_t
	getBaseaddr() const
	{
		assert(card != nullptr);
		return reinterpret_cast<uintptr_t>(card->map) + this->baseaddr;
	}

protected:
	// populated by FpgaIpFactory
	PCIeCard* card;		/**< FPGA card this IP is instantiated on */
	std::string name;	/**< Name defined in JSON config */
	Vlnv vlnv;		/**< VLNV defined in JSON config */
	uintptr_t baseaddr;	/**< The baseadress of this FPGA IP component */
	int irq;			/**< The interrupt number of the FPGA IP component. */
	int port;			/**< The port of the AXI4-Stream switch to which this FPGA IP component is connected. */
};


class IpCoreFactory : public Plugin {
public:
	IpCoreFactory(std::string concreteName) :
	    Plugin(std::string("FPGA IpCore Factory: ") + concreteName)
	{ pluginType = Plugin::Type::FpgaIp; }

	/// Returns a running and checked FPGA IP
	static IpCore*
	make(PCIeCard* card, json_t *json, std::string name);

private:
	/// Create a concrete IP instance
	virtual IpCore* create() = 0;

	/// Configure IP instance from JSON config
	virtual bool configureJson(IpCore* ip, json_t *json)
	{ return true; }

	virtual Vlnv getCompatibleVlnv() const = 0;
	virtual std::string getName() const = 0;
	virtual std::string getDescription() const = 0;

private:
	static IpCoreFactory*
	lookup(const Vlnv& vlnv);
};

/** @} */

} // namespace ip
} // namespace fpga
} // namespace villas
