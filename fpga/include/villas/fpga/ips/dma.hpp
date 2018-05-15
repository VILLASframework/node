/** DMA driver
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2018, RWTH Institute for Automation of Complex Power Systems (ACS)
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
 ******************************************************************************/

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <list>
#include <string>

#include <xilinx/xaxidma.h>

#include "fpga/ip_node.hpp"
#include "memory.hpp"

namespace villas {
namespace fpga {
namespace ip {

class Dma : public IpNode
{
public:
	friend class DmaFactory;

	bool init();
	bool reset();

	size_t write(const MemoryBlock& mem, size_t len);
	size_t read(const MemoryBlock& mem, size_t len);

	bool writeComplete()
	{ return hasScatterGather() ? writeCompleteSG() : writeCompleteSimple(); }

	bool readComplete()
	{ return hasScatterGather() ? readCompleteSG() : readCompleteSimple(); }

	bool memcpy(const MemoryBlock& src, const MemoryBlock& dst, size_t len);

	bool makeAccesibleFromVA(const MemoryBlock& mem);

	inline bool
	hasScatterGather() const
	{ return hasSG; }

private:
	size_t writeSG(const void* buf, size_t len);
	size_t readSG(void* buf, size_t len);
	bool writeCompleteSG();
	bool readCompleteSG();

	size_t writeSimple(const void* buf, size_t len);
	size_t readSimple(void* buf, size_t len);
	bool writeCompleteSimple();
	bool readCompleteSimple();

	bool isMemoryBlockAccesible(const MemoryBlock& mem, const std::string& interface);

private:
	static constexpr char registerMemory[] = "Reg";

	static constexpr char mm2sInterrupt[] = "mm2s_introut";
	static constexpr char mm2sInterface[] = "M_AXI_MM2S";

	static constexpr char s2mmInterrupt[] = "s2mm_introut";
	static constexpr char s2mmInterface[] = "M_AXI_S2MM";

	// optional Scatter-Gather interface to access descriptors
	static constexpr char sgInterface[] = "M_AXI_SG";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{ return { registerMemory }; }

	XAxiDma xDma;
	bool hasSG;
};



class DmaFactory : public IpNodeFactory {
public:
	DmaFactory();

	IpCore* create()
	{ return new Dma; }

	std::string
	getName() const
	{ return "Dma"; }

	std::string
	getDescription() const
	{ return "Xilinx's AXI4 Direct Memory Access Controller"; }

	Vlnv getCompatibleVlnv() const
	{ return {"xilinx.com:ip:axi_dma:"}; }
};

} // namespace ip
} // namespace fpga
} // namespace villas

/** @} */
