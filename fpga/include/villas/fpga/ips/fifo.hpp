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

#include <xilinx/xllfifo.h>

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Fifo : public Node {
public:
	friend class FifoFactory;

	virtual
	bool init() override;

	virtual
	bool stop() override;

	size_t write(const void* buf, size_t len);
	size_t read(void* buf, size_t len);

private:
	static constexpr char registerMemory[] = "Mem0";
	static constexpr char axi4Memory[] = "Mem1";
	static constexpr char irqName[] = "interrupt";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{
		return {
			registerMemory,
			axi4Memory
		};
	}

	XLlFifo xFifo;
};

class FifoData : public Node {
	friend class FifoDataFactory;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
