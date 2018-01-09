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

class AxiStreamSwitch : public IpNode {
public:
	friend class AxiStreamSwitchFactory;

	bool start();

	bool connect(int portSlave, int portMaster);
	bool disconnectMaster(int port);
	bool disconnectSlave(int port);

private:
	static constexpr int PORT_DISABLED = -1;

	struct Path {
		IpCore* masterOut;
		IpCore* slaveIn;
	};

	XAxis_Switch xilinxDriver;
	std::map<int, int> portMapping;
};


class AxiStreamSwitchFactory : public IpNodeFactory {
public:
	AxiStreamSwitchFactory() :
	    IpNodeFactory(getName()) {}

	IpCore* create()
	{ return new AxiStreamSwitch; }

	std::string	getName() const
	{ return "AxiStreamSwitch"; }

	std::string getDescription() const
	{ return "Xilinx's AXI4-Stream switch"; }

	Vlnv getCompatibleVlnv() const
	{ return Vlnv("xilinx.com:ip:axis_interconnect:"); }
};

} // namespace ip
} // namespace fpga
} // namespace villas

/** @} */
