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

#include <vector>

namespace villas {

//enum fpga_ip_types {
//	FPGA_IP_TYPE_DM_DMA,	/**< A datamover IP exchanges streaming data between the FPGA and the CPU. */
//	FPGA_IP_TYPE_DM_FIFO,	/**< A datamover IP exchanges streaming data between the FPGA and the CPU. */
//	FPGA_IP_TYPE_MODEL,	/**< A model IP simulates a system on the FPGA. */
//	FPGA_IP_TYPE_MATH,	/**< A math IP performs some kind of mathematical operation on the streaming data */
//	FPGA_IP_TYPE_MISC,	/**< Other IP components like timer, counters, interrupt conctrollers or routing. */
//	FPGA_IP_TYPE_INTERFACE	/**< A interface IP connects the FPGA to another system or controller. */
//} type;


class FpgaIpFactory;

class FpgaIp {
public:

	friend FpgaIpFactory;

	FpgaIp() : card(nullptr), baseaddr(0), irq(-1), port(-1) {}
	virtual ~FpgaIp() {}

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
	FpgaCard* card;		/**< FPGA card this IP is instantiated on */
	std::string name;	/**< Name defined in JSON config */
	FpgaVlnv vlnv;		/**< VLNV defined in JSON config */
	uintptr_t baseaddr;	/**< The baseadress of this FPGA IP component */
	int irq;			/**< The interrupt number of the FPGA IP component. */
	int port;			/**< The port of the AXI4-Stream switch to which this FPGA IP component is connected. */
};


class FpgaIpFactory : public Plugin {
public:
	FpgaIpFactory(std::string concreteName) :
	    Plugin(std::string("FPGA IP Factory: ") + concreteName)
	{ pluginType = Plugin::Type::FpgaIp; }

	/// Returns a running and checked FPGA IP
	static FpgaIp*
	make(FpgaCard* card, json_t *json, std::string name);

private:
	/// Create a concrete IP instance
	virtual FpgaIp* create() = 0;

	/// Configure IP instance from JSON config
	virtual bool configureJson(FpgaIp* ip, json_t *json)
	{ return true; }

	virtual FpgaVlnv getCompatibleVlnv() const = 0;
	virtual std::string getName() const = 0;
	virtual std::string getDescription() const = 0;

private:
	static FpgaIpFactory*
	lookup(const FpgaVlnv& vlnv);
};

/** @} */

} // namespace villas
