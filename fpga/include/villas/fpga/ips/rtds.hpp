/** Driver for AXI Stream wrapper around RTDS_InterfaceModule (rtds_axis )
 *
 * @file
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
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
