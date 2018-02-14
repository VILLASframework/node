/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017, Steffen Vogel
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

#include <string>
#include <map>

#include <jansson.h>
#include <xilinx/xaxis_switch.h>

#include "fpga/ip_node.hpp"
#include "fpga/vlnv.hpp"

namespace villas {
namespace fpga {
namespace ip {

class AxiPciExpressBridge : public IpCore {
public:
	friend class AxiPciExpressBridgeFactory;

	bool init();
};


class AxiPciExpressBridgeFactory : public IpCoreFactory {
public:
	AxiPciExpressBridgeFactory() :
	    IpCoreFactory(getName()) {}

	static constexpr const char*
	getCompatibleVlnvString()
	{ return "xilinx.com:ip:axi_pcie:"; }

	IpCore* create()
	{ return new AxiPciExpressBridge; }

	std::string	getName() const
	{ return "AxiPciExpressBridge"; }

	std::string getDescription() const
	{ return "Xilinx's AXI-PCIe Bridge"; }

	Vlnv getCompatibleVlnv() const
	{ return Vlnv(getCompatibleVlnvString()); }
};

} // namespace ip
} // namespace fpga
} // namespace villas

/** @} */
