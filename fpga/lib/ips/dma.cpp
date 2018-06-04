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

#include <sstream>
#include <string>

#include <xilinx/xaxidma.h>

#include <villas/memory.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/intc.hpp>

// max. size of a DMA transfer in simple mode
#define FPGA_DMA_BOUNDARY	0x1000


namespace villas {
namespace fpga {
namespace ip {

// instantiate factory to make available to plugin infrastructure
static DmaFactory factory;

DmaFactory::DmaFactory() :
    IpNodeFactory(getName())
{
	// nothing to do
}


bool
Dma::init()
{
	// if there is a scatter-gather interface, then this instance has it
	hasSG = busMasterInterfaces.count(sgInterface) == 1;
	logger->info("Scatter-Gather support: {}", hasScatterGather());

	XAxiDma_Config xdma_cfg;

	xdma_cfg.BaseAddr = getBaseAddr(registerMemory);
	xdma_cfg.HasStsCntrlStrm = 0;
	xdma_cfg.HasMm2S = 1;
	xdma_cfg.HasMm2SDRE = 1;
	xdma_cfg.Mm2SDataWidth = 128;
	xdma_cfg.HasS2Mm = 1;
	xdma_cfg.HasS2MmDRE = 1; /* Data Realignment Engine */
	xdma_cfg.HasSg = hasScatterGather();
	xdma_cfg.S2MmDataWidth = 128;
	xdma_cfg.Mm2sNumChannels = 1;
	xdma_cfg.S2MmNumChannels = 1;
	xdma_cfg.Mm2SBurstSize = 64;
	xdma_cfg.S2MmBurstSize = 64;
	xdma_cfg.MicroDmaMode = 0;
	xdma_cfg.AddrWidth = 32;

	if (XAxiDma_CfgInitialize(&xDma, &xdma_cfg) != XST_SUCCESS) {
		logger->error("Cannot initialize Xilinx DMA driver");
		return false;
	}

	if (XAxiDma_Selftest(&xDma) != XST_SUCCESS) {
		logger->error("DMA selftest failed");
		return false;
	} else {
		logger->debug("DMA selftest passed");
	}

	/* Map buffer descriptors */
	if (hasScatterGather()) {
		logger->warn("Scatter Gather not yet implemented");
		return false;

//		ret = dma_alloc(c, &dma->bd, FPGA_DMA_BD_SIZE, 0);
//		if (ret)
//			return -3;

//		ret = dma_init_rings(&xDma, &dma->bd);
//		if (ret)
//			return -4;
	}

	/* Enable completion interrupts for both channels */
	XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	irqs[mm2sInterrupt].irqController->enableInterrupt(irqs[mm2sInterrupt], false);
	irqs[s2mmInterrupt].irqController->enableInterrupt(irqs[s2mmInterrupt], false);

	return true;
}


bool
Dma::reset()
{
	XAxiDma_Reset(&xDma);

	// value taken from libxil implementation
	int timeout = 500;

	while(timeout > 0) {
		if(XAxiDma_ResetIsDone(&xDma))
			return true;

		timeout--;
	}

	return false;
}


bool
Dma::memcpy(const MemoryBlock& src, const MemoryBlock& dst, size_t len)
{
	if(len == 0)
		return true;

	if(not connectLoopback())
		return false;

	if(this->read(dst, len) == 0)
		return false;

	if(this->write(src, len) == 0)
		return false;

	if(not this->writeComplete())
		return false;

	if(not this->readComplete())
		return false;

	return true;
}


size_t
Dma::write(const MemoryBlock& mem, size_t len)
{
	auto& mm = MemoryManager::get();

	// user has to make sure that memory is accessible, otherwise this will throw
	auto translation = mm.getTranslation(busMasterInterfaces[mm2sInterface],
	                                     mem.getAddrSpaceId());
	const void* buf = reinterpret_cast<void*>(translation.getLocalAddr(0));

	logger->debug("Write to address: {:p}", buf);
	return hasScatterGather() ? writeSG(buf, len) : writeSimple(buf, len);
}


size_t
Dma::read(const MemoryBlock& mem, size_t len)
{
	auto& mm = MemoryManager::get();

	// user has to make sure that memory is accessible, otherwise this will throw
	auto translation = mm.getTranslation(busMasterInterfaces[s2mmInterface],
	                                     mem.getAddrSpaceId());
	void* buf = reinterpret_cast<void*>(translation.getLocalAddr(0));

	logger->debug("Read from address: {:p}", buf);
	return hasScatterGather() ? readSG(buf, len) : readSimple(buf, len);
}


size_t
Dma::writeSG(const void* buf, size_t len)
{
	(void) buf;
	(void) len;
	logger->error("DMA Scatter Gather write not implemented");

	return 0;
}


size_t
Dma::readSG(void* buf, size_t len)
{
	(void) buf;
	(void) len;
	logger->error("DMA Scatter Gather read not implemented");

	return 0;
}


bool
Dma::writeCompleteSG()
{
	logger->error("DMA Scatter Gather write not implemented");

	return false;
}


bool
Dma::readCompleteSG()
{
	logger->error("DMA Scatter Gather read not implemented");

	return false;
}


size_t
Dma::writeSimple(const void *buf, size_t len)
{
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(&xDma);

	if ((len == 0) || (len > FPGA_DMA_BOUNDARY))
		return 0;

	if (not ring->HasDRE) {
		const uint32_t mask = xDma.MicroDmaMode
		                      ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
		                      : ring->DataWidth - 1;

		if (reinterpret_cast<uintptr_t>(buf) & mask) {
			return 0;
		}
	}

	const bool dmaChannelHalted =
	        XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;

	const bool dmaToDeviceBusy = XAxiDma_Busy(&xDma, XAXIDMA_DMA_TO_DEVICE);

	/* If the engine is doing a transfer, cannot submit  */
	if (not dmaChannelHalted and dmaToDeviceBusy) {
		return 0;
	}

	// set lower 32 bit of source address
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_OFFSET,
	                 LOWER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// if neccessary, set upper 32 bit of source address
	if (xDma.AddrWidth > 32) {
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_MSB_OFFSET,
		                 UPPER_32_BITS(reinterpret_cast<uintptr_t>(buf)));
	}

	// start DMA channel
	auto channelControl = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET);
	channelControl |= XAXIDMA_CR_RUNSTOP_MASK;
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_CR_OFFSET, channelControl);

	// set tail descriptor pointer
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET, len);


	return len;
}


size_t
Dma::readSimple(void *buf, size_t len)
{
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(&xDma);

	if ((len == 0) || (len > FPGA_DMA_BOUNDARY))
		return 0;

	if (not ring->HasDRE) {
		const uint32_t mask = xDma.MicroDmaMode
		                      ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
		                      : ring->DataWidth - 1;

		if (reinterpret_cast<uintptr_t>(buf) & mask) {
			return 0;
		}
	}

	const bool dmaChannelHalted =
	        XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;

	const bool deviceToDmaBusy = XAxiDma_Busy(&xDma, XAXIDMA_DEVICE_TO_DMA);

	/* If the engine is doing a transfer, cannot submit  */
	if (not dmaChannelHalted and deviceToDmaBusy) {
		return 0;
	}

	// set lower 32 bit of destination address
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_OFFSET,
	                 LOWER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// if neccessary, set upper 32 bit of destination address
	if (xDma.AddrWidth > 32)
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_MSB_OFFSET,
		                 UPPER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// start DMA channel
	auto channelControl = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET);
	channelControl |= XAXIDMA_CR_RUNSTOP_MASK;
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_CR_OFFSET, channelControl);

	// set tail descriptor pointer
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET, len);

	return len;
}


bool
Dma::writeCompleteSimple()
{
	while (!(XAxiDma_IntrGetIrq(&xDma, XAXIDMA_DMA_TO_DEVICE) & XAXIDMA_IRQ_IOC_MASK))
		irqs[mm2sInterrupt].irqController->waitForInterrupt(irqs[mm2sInterrupt]);

	XAxiDma_IntrAckIrq(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);

	return true;
}


bool
Dma::readCompleteSimple()
{
	while (!(XAxiDma_IntrGetIrq(&xDma, XAXIDMA_DEVICE_TO_DMA) & XAXIDMA_IRQ_IOC_MASK))
		irqs[s2mmInterrupt].irqController->waitForInterrupt(irqs[s2mmInterrupt]);

	XAxiDma_IntrAckIrq(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	return true;
}


bool
Dma::makeAccesibleFromVA(const MemoryBlock& mem)
{
	// only symmetric mapping supported currently
	if(isMemoryBlockAccesible(mem, s2mmInterface) and
	   isMemoryBlockAccesible(mem, mm2sInterface)) {
		return true;
	}

	// try mapping via FPGA-card (VFIO)
	if(not card->mapMemoryBlock(mem)) {
		logger->error("Memory not accessible by DMA");
		return false;
	}

	// sanity-check if mapping worked, this shouldn't be neccessary
	if(not isMemoryBlockAccesible(mem, s2mmInterface) or
	   not isMemoryBlockAccesible(mem, mm2sInterface)) {
		logger->error("Mapping memory via card didn't work, but reported success?!");
		return false;
	}

	return true;
}


bool
Dma::isMemoryBlockAccesible(const MemoryBlock& mem, const std::string& interface)
{
	auto& mm = MemoryManager::get();

	try {
		mm.findPath(getMasterAddrSpaceByInterface(interface), mem.getAddrSpaceId());
	} catch(const std::out_of_range&) {
		// not (yet) accessible
		return false;
	}

	return true;
}


} // namespace ip
} // namespace fpga
} // namespace villas
