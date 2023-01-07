/** AXI General Purpose IO (GPIO)
 *
 * @file
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017-2020, Steffen Vogel
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

#include <villas/fpga/core.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Gpio : public Core {
public:

	virtual
	bool init() override;

private:

	static constexpr char registerMemory[] = "Reg";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{
		return {
			registerMemory
		};
	}
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
