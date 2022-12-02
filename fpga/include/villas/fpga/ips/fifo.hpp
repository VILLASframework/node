/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
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
