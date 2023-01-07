/** AXI External Memory Controller (EMC)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

#include <xilinx/xilflash.h>

#include <villas/fpga/core.hpp>

namespace villas {
namespace fpga {
namespace ip {

class EMC : public Core {
public:

	virtual
	bool init() override;

	bool flash(uint32_t offset, const std::string &filename);
	bool flash(uint32_t offset, uint32_t length, uint8_t *data);

	bool read(uint32_t offset, uint32_t length, uint8_t *data);

private:

	XFlash xflash;

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
