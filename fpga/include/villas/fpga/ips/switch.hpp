/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2022, Steffen Vogel
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

#pragma once

#include <map>

#include <xilinx/xaxis_switch.h>

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class AxiStreamSwitch : public Node {
public:
	friend class AxiStreamSwitchFactory;

	virtual
	bool init() override;

	bool connectInternal(const std::string &slavePort,
	                     const std::string &masterPort);

private:
	int portNameToNum(const std::string &portName);

private:
	static constexpr
	const char* PORT_DISABLED = "DISABLED";

	static constexpr
	char registerMemory[] = "Reg";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{
		return {
			registerMemory
		};
	}

	XAxis_Switch xSwitch;
	XAxis_Switch_Config xConfig;

	std::map<std::string, std::string> portMapping;
};

class AxiStreamSwitchFactory : public NodeFactory {
public:

	static constexpr
	const char* getCompatibleVlnvString()
	{
		return "xilinx.com:ip:axis_switch:";
	}

	void parse(Core &ip, json_t *cfg);

	Core* create()
	{
		return new AxiStreamSwitch;
	}

	virtual
	std::string getName() const
	{
		return "AxiStreamSwitch";
	}

	virtual
	std::string getDescription() const
	{
		return "Xilinx's AXI4-Stream switch";
	}

	virtual
	Vlnv getCompatibleVlnv() const
	{
		return Vlnv(getCompatibleVlnvString());
	}
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
