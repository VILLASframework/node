/** Driver for AXI Stream wrapper around RTDS_InterfaceModule (rtds_axis )
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
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

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class RtdsGtfpga : public Node {
public:
	static constexpr const char* masterPort = "m_axis";
	static constexpr const char* slavePort = "s_axis";

	virtual
	void dump() override;

	double getDt();

	std::list<std::string> getMemoryBlocks() const
	{
		return {
			registerMemory
		};
	}

	const StreamVertex&
	getDefaultSlavePort() const
	{
		return getSlavePort(slavePort);
	}

	const StreamVertex&
	getDefaultMasterPort() const
	{
		return getMasterPort(masterPort);
	}

private:
	static constexpr const char registerMemory[] = "reg0";
	static constexpr const char* irqTs = "irq_ts";
	static constexpr const char* irqOverflow = "irq_overflow";
	static constexpr const char* irqCase = "irq_case";
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
