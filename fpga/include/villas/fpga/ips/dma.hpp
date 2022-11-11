/** DMA driver
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @copyright 2018-2022, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <xilinx/xaxidma.h>

#include <villas/memory.hpp>
#include <villas/fpga/node.hpp>
#include <villas/exceptions.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Dma : public Node
{
public:
	friend class DmaFactory;

	~Dma();
	virtual
	bool init() override;

	virtual
	bool reset() override;

	// Memory-mapped to stream (MM2S)
	bool write(const MemoryBlock &mem, size_t len);

	// Stream to memory-mapped (S2MM)
	bool read(const MemoryBlock &mem, size_t len);

	size_t writeComplete()
	{
		return hasScatterGather() ? writeCompleteScatterGather() : writeCompleteSimple();
	}

	size_t readComplete()
	{
		return hasScatterGather() ? readCompleteScatterGather() : readCompleteSimple();
	}

	bool memcpy(const MemoryBlock &src, const MemoryBlock &dst, size_t len);

	void makeAccesibleFromVA(const MemoryBlock &mem);
	bool makeInaccesibleFromVA(const MemoryBlock &mem);

	inline
	bool hasScatterGather() const
	{
		return xConfig.HasSg;
	}

	const StreamVertex& getDefaultSlavePort() const
	{
		return getSlavePort(s2mmPort);
	}

	const StreamVertex& getDefaultMasterPort() const
	{
		return getMasterPort(mm2sPort);
	}

private:
	bool writeScatterGather(const void* buf, size_t len);
	bool readScatterGather(void* buf, size_t len);
	size_t writeCompleteScatterGather();
	size_t readCompleteScatterGather();

	bool writeSimple(const void* buf, size_t len);
	bool readSimple(void* buf, size_t len);
	size_t writeCompleteSimple();
	size_t readCompleteSimple();

	void setupScatterGather();
	void setupScatterGatherRingRx();
	void setupScatterGatherRingTx();

public:
	static constexpr const char* s2mmPort = "S2MM";
	static constexpr const char* mm2sPort = "MM2S";

	bool isMemoryBlockAccesible(const MemoryBlock &mem, const std::string &interface);

	virtual
	void dump() override;

private:
	static constexpr char registerMemory[] = "Reg";

	static constexpr char mm2sInterrupt[] = "mm2s_introut";
	static constexpr char mm2sInterface[] = "M_AXI_MM2S";

	static constexpr char s2mmInterrupt[] = "s2mm_introut";
	static constexpr char s2mmInterface[] = "M_AXI_S2MM";

	// Optional Scatter-Gather interface to access descriptors
	static constexpr char sgInterface[] = "M_AXI_SG";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{
		return {
			registerMemory
		};
	}

	XAxiDma xDma;
	XAxiDma_Config xConfig;


	bool configDone = false;
	// use polling to wait for DMA completion or interrupts via efds
	bool polling = false;
	// Timeout after which the DMA controller issues in interrupt if no data has been received
	// Delay is 125 x <delay> x (clock period of SG clock). SG clock is 100 MHz by default.
	int delay = 0;
	// Coalesce is the number of messages/BDs to wait for before issuing an interrupt
	uint32_t writeCoalesce = 1;
	uint32_t readCoalesce = 16;

	// (maximum) size of a single message on the read channel in bytes.
	// The message buffer/BD should have enough room for this many bytes.
	size_t readMsgSize = 4;

	// When using SG: ringBdSize is the maximum number of BDs usable in the ring
	// Depending on alignment, the actual number of BDs usable can be smaller
	static constexpr size_t requestedRingBdSize = 2048;
	uint32_t actualRingBdSize = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, requestedRingBdSize);
	MemoryBlock::UniquePtr sgRingTx;
	MemoryBlock::UniquePtr sgRingRx;
};

class DmaFactory : public NodeFactory {
public:

	Core* create()
	{
		return new Dma;
	}

	virtual
	std::string getName() const
	{
		return "Dma";
	}

	virtual
	std::string getDescription() const
	{
		return "Xilinx's AXI4 Direct Memory Access Controller";
	}

	virtual
	Vlnv getCompatibleVlnv() const
	{
		return Vlnv("xilinx.com:ip:axi_dma:");
	}

	virtual
	void parse(Core& ip, json_t* json) override;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
