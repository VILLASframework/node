/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Steffen Vogel
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <xilinx/xllfifo.h>

#include <villas/fpga/ip_node.hpp>


namespace villas {
namespace fpga {
namespace ip {


class Fifo : public IpNode
{
public:
	friend class FifoFactory;

	bool init();
	bool stop();

	size_t write(const void* buf, size_t len);
	size_t read(void* buf, size_t len);

private:
	static constexpr char registerMemory[] = "Mem0";
	static constexpr char axi4Memory[] = "Mem1";
	static constexpr char irqName[] = "interrupt";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{ return { registerMemory, axi4Memory }; }

	XLlFifo xFifo;
};



class FifoFactory : public IpNodeFactory {
public:

	IpCore* create()
	{ return new Fifo; }

	std::string
	getName() const
	{ return "Fifo"; }

	std::string
	getDescription() const
	{ return "Xilinx's AXI4 FIFO data mover"; }

	Vlnv getCompatibleVlnv() const
	{ return {"xilinx.com:ip:axi_fifo_mm_s:"}; }
};


class FifoData : public IpNode {
	friend class FifoDataFactory;
};


class FifoDataFactory : public IpNodeFactory {
public:

	IpCore* create()
	{ return new FifoData; }

	virtual std::string
	getName() const
	{ return "FifoData"; }

	virtual std::string
	getDescription() const
	{ return "Xilinx's AXI4 data stream FIFO"; }

	virtual Vlnv
	getCompatibleVlnv() const
	{ return {"xilinx.com:ip:axis_data_fifo:"}; }
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */

/** @} */
