/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @file
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017-2022, Steffen Vogel
 * SPDX-License-Identifier: Apache-2.0
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

class AxiStreamSwitchFactory : NodeFactory {

public:
	virtual
	std::string getName() const
	{
		return "switch";
	}

	virtual
	std::string getDescription() const
	{
		return "Xilinx's AXI4-Stream switch";
	}

private:
	virtual
	Vlnv getCompatibleVlnv() const
	{
		return Vlnv("xilinx.com:ip:axis_switch:");
	}

	// Create a concrete IP instance
	Core* make() const
	{
		return new AxiStreamSwitch;
	};

protected:
	virtual
	void parse(Core &, json_t *) override;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
