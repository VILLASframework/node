/** DMA driver
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <sstream>
#include <string>

#include <xilinx/xaxidma.h>

#include <villas/exceptions.hpp>
#include <villas/memory.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/intc.hpp>

// Max. size of a DMA transfer in simple mode
#define FPGA_DMA_BOUNDARY	0x1000

using namespace villas::fpga::ip;

// Instantiate factory to make available to plugin infrastructure
static DmaFactory factory;

bool
Dma::init()
{
	// If there is a scatter-gather interface, then this instance has it
	hasSG = busMasterInterfaces.count(sgInterface) == 1;
	logger->info("Scatter-Gather support: {}", hasScatterGather());

	XAxiDma_Config xdma_cfg;

	xdma_cfg.BaseAddr = getBaseAddr(registerMemory);
	xdma_cfg.HasStsCntrlStrm = 0;
	xdma_cfg.HasMm2S = 1;
	xdma_cfg.HasMm2SDRE = 0;
	xdma_cfg.Mm2SDataWidth = 32;
	xdma_cfg.HasS2Mm = 1;
	xdma_cfg.HasS2MmDRE = 0;
	xdma_cfg.HasSg = hasScatterGather();
	xdma_cfg.S2MmDataWidth = 32;
	xdma_cfg.Mm2sNumChannels = 1;
	xdma_cfg.S2MmNumChannels = 1;
	xdma_cfg.Mm2SBurstSize = 16;
	xdma_cfg.S2MmBurstSize = 16;
	xdma_cfg.MicroDmaMode = 0;
	xdma_cfg.AddrWidth = 32;

	if (XAxiDma_CfgInitialize(&xDma, &xdma_cfg) != XST_SUCCESS) {
		logger->error("Cannot initialize Xilinx DMA driver");
		return false;
	}

	if (XAxiDma_Selftest(&xDma) != XST_SUCCESS) {
		logger->error("DMA selftest failed");
		return false;
	}
	else
		logger->debug("DMA selftest passed");

	// Map buffer descriptors
	if (hasScatterGather()) {
		auto &alloc = villas::HostRam::getAllocator();
		auto mem = alloc.allocate<uint8_t>(0x2000);

		sgRings = mem.getMemoryBlock();

		makeAccesibleFromVA(sgRings);

		setupRingRx();
		setupRingTx();
	}

	// Enable completion interrupts for both channels
	XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	irqs[mm2sInterrupt].irqController->enableInterrupt(irqs[mm2sInterrupt], true);
	irqs[s2mmInterrupt].irqController->enableInterrupt(irqs[s2mmInterrupt], true);

	return true;
}

void Dma::setupRingRx()
{
	XAxiDma_BdRing *RxRingPtr;

	int Delay = 0;
	int Coalesce = 1;
	int Status;

	XAxiDma_Bd BdTemplate;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;

	u32 BdCount;
	u32 FreeBdCount;
	UINTPTR RxBufferPtr;
	int Index;

	RxRingPtr = XAxiDma_GetRxRing(&xDma);

	/* Disable all RX interrupts before RxBD space setup */
	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Set delay and coalescing */
	XAxiDma_BdRingSetCoalesce(RxRingPtr, Coalesce, Delay);

	/* Setup Rx BD space */
	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, 0x1000);

	Status = XAxiDma_BdRingCreate(RxRingPtr, sgRings., RX_BD_SPACE_BASE, XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);

	if (Status != XST_SUCCESS)
		throw RuntimeError("RX create BD ring failed {}", Status);

	/*
	 * Setup an all-zero BD as the template for the Rx channel.
	 */
	XAxiDma_BdClear(&BdTemplate);

	Status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS)
		throw RuntimeError("RX clone BD failed {}", Status);

	/* Attach buffers to RxBD ring so we are ready to receive packets */

	FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);

	Status = XAxiDma_BdRingAlloc(RxRingPtr, FreeBdCount, &BdPtr);
	if (Status != XST_SUCCESS)
		throw RuntimeError("RX alloc BD failed {}", Status);

	BdCurPtr = BdPtr;
	RxBufferPtr = RX_BUFFER_BASE;
	for (Index = 0; Index < FreeBdCount; Index++) {
		Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);

		if (Status != XST_SUCCESS)
			throw RuntimeError("Set buffer addr {} on BD {} failed {}", (unsigned int)RxBufferPtr, (UINTPTR)BdCurPtr, Status);

		Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN, RxRingPtr->MaxTransferLen);
		if (Status != XST_SUCCESS)
			throw RuntimeError("Rx set length {} on BD {} failed {}", MAX_PKT_LEN, (UINTPTR)BdCurPtr, Status);

		/* Receive BDs do not need to set anything for the control
		 * The hardware will set the SOF/EOF bits per stream status
		 */
		XAxiDma_BdSetCtrl(BdCurPtr, 0);
		XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);

		RxBufferPtr += MAX_PKT_LEN;
		BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
	}

	/* Clear the receive buffer, so we can verify data
	 */
	memset((void *)RX_BUFFER_BASE, 0, MAX_PKT_LEN);

	Status = XAxiDma_BdRingToHw(RxRingPtr, FreeBdCount, BdPtr);
	if (Status != XST_SUCCESS)
		throw RuntimeError("RX submit hw failed {}", Status);

	/* Start RX DMA channel */
	Status = XAxiDma_BdRingStart(RxRingPtr);
	if (Status != XST_SUCCESS)
		throw RuntimeError("RX start hw failed {}", Status);
}

void Dma::setupRingTx()
{
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_Bd BdTemplate;

	int Delay = 0;
	int Coalesce = 1;

	int Status;
	u32 BdCount;

	TxRingPtr = XAxiDma_GetTxRing(&xDma);

	/* Disable all TX interrupts before TxBD space setup */
	XAxiDma_BdRingIntDisable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Set TX delay and coalesce */
	XAxiDma_BdRingSetCoalesce(TxRingPtr, Coalesce, Delay);

	/* Setup TxBD space  */
	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, TX_BD_SPACE_HIGH - TX_BD_SPACE_BASE + 1);

	Status = XAxiDma_BdRingCreate(TxRingPtr, TX_BD_SPACE_BASE, TX_BD_SPACE_BASE, XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS)
		throw RuntimeError("failed create BD ring in txsetup");

	/*
	 * We create an all-zero BD as the template.
	 */
	XAxiDma_BdClear(&BdTemplate);

	Status = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS)
		throw RuntimeError("failed bdring clone in txsetup {}", Status);

	/* Start the TX channel */
	Status = XAxiDma_BdRingStart(TxRingPtr);
	if (Status != XST_SUCCESS)
		throw RuntimeError("failed start bdring txsetup {}", Status);
}

bool
Dma::reset()
{
	logger->info("DMA resetted");

	XAxiDma_Reset(&xDma);

	// Value taken from libxil implementation
	int timeout = 500;

	while (timeout > 0) {
		if (XAxiDma_ResetIsDone(&xDma))
			return true;

		timeout--;
	}

	return false;
}

bool
Dma::memcpy(const MemoryBlock &src, const MemoryBlock &dst, size_t len)
{
	if (len == 0)
		return true;

	if (not connectLoopback())
		return false;

	if (this->read(dst, len) == 0)
		return false;

	if (this->write(src, len) == 0)
		return false;

	if (not this->writeComplete())
		return false;

	if (not this->readComplete())
		return false;

	return true;
}

bool
Dma::write(const MemoryBlock &mem, size_t len)
{
	auto &mm = MemoryManager::get();

	// User has to make sure that memory is accessible, otherwise this will throw
	auto translation = mm.getTranslation(busMasterInterfaces[mm2sInterface],
	                                     mem.getAddrSpaceId());
	const void* buf = reinterpret_cast<void*>(translation.getLocalAddr(0));

	logger->debug("Write to stream from address {:p}", buf);
	return hasScatterGather() ? writeSG(buf, len) : writeSimple(buf, len);
}

bool
Dma::read(const MemoryBlock &mem, size_t len)
{
	auto &mm = MemoryManager::get();

	// User has to make sure that memory is accessible, otherwise this will throw
	auto translation = mm.getTranslation(busMasterInterfaces[s2mmInterface],
	                                     mem.getAddrSpaceId());
	void* buf = reinterpret_cast<void*>(translation.getLocalAddr(0));

	logger->debug("Read from stream and write to address {:p}", buf);
	return hasScatterGather() ? readSG(buf, len) : readSimple(buf, len);
}

bool
Dma::writeSG(const void* buf, size_t len)
{
	(void) buf;
	(void) len;
	logger->error("DMA Scatter Gather write not implemented");

	return false;
}

bool
Dma::readSG(void* buf, size_t len)
{
	(void) buf;
	(void) len;
	logger->error("DMA Scatter Gather read not implemented");

	return false;
}

size_t
Dma::writeCompleteSG()
{
	logger->error("DMA Scatter Gather write not implemented");

	return 0;
}

size_t
Dma::readCompleteSG()
{
	logger->error("DMA Scatter Gather read not implemented");

	return 0;
}

bool
Dma::writeSimple(const void *buf, size_t len)
{
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(&xDma);

	if ((len == 0) || (len > FPGA_DMA_BOUNDARY))
		return false;

	if (not ring->HasDRE) {
		const uint32_t mask = xDma.MicroDmaMode
		                      ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
		                      : ring->DataWidth - 1;

		if (reinterpret_cast<uintptr_t>(buf) & mask) {
			return false;
		}
	}

	const bool dmaChannelHalted =
	        XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;

	const bool dmaToDeviceBusy = XAxiDma_Busy(&xDma, XAXIDMA_DMA_TO_DEVICE);

	// If the engine is doing a transfer, cannot submit
	if (not dmaChannelHalted and dmaToDeviceBusy) {
		return false;
	}

	// Set lower 32 bit of source address
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_OFFSET,
	                 LOWER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// If neccessary, set upper 32 bit of source address
	if (xDma.AddrWidth > 32) {
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_MSB_OFFSET,
		                 UPPER_32_BITS(reinterpret_cast<uintptr_t>(buf)));
	}

	// Start DMA channel
	auto channelControl = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET);
	channelControl |= XAXIDMA_CR_RUNSTOP_MASK;
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_CR_OFFSET, channelControl);

	// Set tail descriptor pointer
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET, len);

	return true;
}

bool
Dma::readSimple(void *buf, size_t len)
{
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(&xDma);

	if ((len == 0) || (len > FPGA_DMA_BOUNDARY))
		return false;

	if (not ring->HasDRE) {
		const uint32_t mask = xDma.MicroDmaMode
		                      ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
		                      : ring->DataWidth - 1;

		if (reinterpret_cast<uintptr_t>(buf) & mask) {
			return false;
		}
	}

	const bool dmaChannelHalted =
	        XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;

	const bool deviceToDmaBusy = XAxiDma_Busy(&xDma, XAXIDMA_DEVICE_TO_DMA);

	// If the engine is doing a transfer, cannot submit
	if (not dmaChannelHalted and deviceToDmaBusy) {
		return false;
	}

	// Set lower 32 bit of destination address
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_OFFSET,
	                 LOWER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// If neccessary, set upper 32 bit of destination address
	if (xDma.AddrWidth > 32)
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_MSB_OFFSET,
		                 UPPER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// Start DMA channel
	auto channelControl = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET);
	channelControl |= XAXIDMA_CR_RUNSTOP_MASK;
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_CR_OFFSET, channelControl);

	// Set tail descriptor pointer
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET, len);

	return true;
}

size_t
Dma::writeCompleteSimple()
{
	while (!(XAxiDma_IntrGetIrq(&xDma, XAXIDMA_DMA_TO_DEVICE) & XAXIDMA_IRQ_IOC_MASK))
		irqs[mm2sInterrupt].irqController->waitForInterrupt(irqs[mm2sInterrupt]);

	XAxiDma_IntrAckIrq(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);

	const XAxiDma_BdRing* ring = XAxiDma_GetTxRing(&xDma);
	const size_t bytesWritten = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);

	return bytesWritten;
}

size_t
Dma::readCompleteSimple()
{
	while (!(XAxiDma_IntrGetIrq(&xDma, XAXIDMA_DEVICE_TO_DMA) & XAXIDMA_IRQ_IOC_MASK))
		irqs[s2mmInterrupt].irqController->waitForInterrupt(irqs[s2mmInterrupt]);

	XAxiDma_IntrAckIrq(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	const XAxiDma_BdRing* ring = XAxiDma_GetRxRing(&xDma);
	const size_t bytesRead = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);

	return bytesRead;
}

void
Dma::makeAccesibleFromVA(const MemoryBlock &mem)
{
	// Only symmetric mapping supported currently
	if (isMemoryBlockAccesible(mem, s2mmInterface) and
	   isMemoryBlockAccesible(mem, mm2sInterface))
		return;

	// Try mapping via FPGA-card (VFIO)
	if (not card->mapMemoryBlock(mem))
		throw RuntimeError("Memory not accessible by DMA");

	// Sanity-check if mapping worked, this shouldn't be neccessary
	if (not isMemoryBlockAccesible(mem, s2mmInterface) or
	    not isMemoryBlockAccesible(mem, mm2sInterface))
		throw RuntimeError("Mapping memory via card didn't work, but reported success?!");
}

bool
Dma::isMemoryBlockAccesible(const MemoryBlock &mem, const std::string &interface)
{
	auto &mm = MemoryManager::get();

	try {
		mm.findPath(getMasterAddrSpaceByInterface(interface), mem.getAddrSpaceId());
	} catch (const std::out_of_range&) {
		return false; // Not (yet) accessible
	}

	return true;
}

void
Dma::dump()
{
	Core::dump();

	logger->info("S2MM_DMACR:  {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_CR_OFFSET));
	logger->info("S2MM_DMASR:  {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_SR_OFFSET));
	logger->info("S2MM_LENGTH: {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_BUFFLEN_OFFSET));
}

