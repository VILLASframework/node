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

#include <xilinx/xaxis_switch.h>

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class AxiPciExpressBridge : public Core {
public:
	friend class AxiPciExpressBridgeFactory;

	virtual
	bool init() override;

private:
	static constexpr char axiInterface[] = "M_AXI";
	static constexpr char pcieMemory[] = "BAR0";

	struct AxiBar {
		uintptr_t base;
		size_t size;
		uintptr_t translation;
	};

	struct PciBar {
		uintptr_t translation;
	};

	std::map<std::string, AxiBar> axiToPcieTranslations;
	std::map<std::string, PciBar> pcieToAxiTranslations;
};

class AxiPciExpressBridgeFactory : CoreFactory {

public:
	virtual
	std::string getName() const
	{
		return "pcie";
	}

	virtual
	std::string getDescription() const
	{
		return "Xilinx's AXI-PCIe Bridge";
	}

private:
	virtual
	Vlnv getCompatibleVlnv() const
	{
		return Vlnv("xilinx.com:ip:axi_pcie:");
	}

	// Create a concrete IP instance
	Core* make() const
	{
		return new AxiPciExpressBridge;
	};

protected:
	virtual
	void parse(Core &, json_t *) override;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
