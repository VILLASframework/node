/** Driver for wrapper around Aurora (acs.eonerc.rwth-aachen.de:user:aurora)
 *
 * @file
 * Author: Hatim Kanchwala <hatim@hatimak.me>
 * SPDX-FileCopyrightText: 2020-2022, Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Aurora : public Node {
public:
	static constexpr const char* masterPort = "m_axis";
	static constexpr const char* slavePort = "s_axis";

	virtual
	void dump() override;

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

	void
	setLoopback(bool state);

	void
	resetFrameCounters();

private:
	static constexpr const char registerMemory[] = "reg0";
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
