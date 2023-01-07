/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017-2022, Steffen Vogel
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

#include <cstdint>
#include <xilinx/xtmrctr.h>

#include <villas/fpga/config.h>
#include <villas/fpga/core.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Timer : public Core {
	friend class TimerFactory;
public:

	virtual
	bool init() override;

	bool start(uint32_t ticks);
	bool wait();
	uint32_t remaining();

	inline
	bool isRunning()
	{
		return remaining() != 0;
	}

	inline
	bool isFinished()
	{
		return remaining() == 0;
	}

	static constexpr
	uint32_t getFrequency()
	{
		return FPGA_AXI_HZ;
	}

private:

	std::list<MemoryBlockName> getMemoryBlocks() const
	{
		return {
			registerMemory
		};
	}

	static constexpr char irqName[] = "generateout0";
	static constexpr char registerMemory[] = "Reg";

	XTmrCtr xTmr;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
