/** Driver for wrapper around Dino
 *
 * Author: Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2020 Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Dino : public Node {
public:
	static constexpr const char* masterPort = "m_axis";
	static constexpr const char* slavePort = "s_axis";

	const StreamVertex& getDefaultSlavePort() const
	{
		return getSlavePort(slavePort);
	}

	const StreamVertex& getDefaultMasterPort() const
	{
		return getMasterPort(masterPort);
	}
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
