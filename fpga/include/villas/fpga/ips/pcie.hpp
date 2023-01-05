/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
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
