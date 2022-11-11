/** DMA driver
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <sstream>
#include <string>

#include <xilinx/xaxidma.h>

#include <villas/memory.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/intc.hpp>

// Max. size of a DMA transfer in simple mode
#define FPGA_DMA_BOUNDARY 0x1000

using namespace villas::fpga::ip;

// Instantiate factory to make available to plugin infrastructure
static DmaFactory factory;

bool Dma::init()
{
	// Check if configJson has been called
	if (!configDone)
		throw RuntimeError("DMA device not configured");

	if (hasScatterGather())
		logger->info("Scatter-Gather support: {}", hasScatterGather());

	xConfig.BaseAddr = getBaseAddr(registerMemory);

	if (XAxiDma_CfgInitialize(&xDma, &xConfig) != XST_SUCCESS) {
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
		if (actualRingBdSize < 2*readCoalesce || actualRingBdSize < 2*writeCoalesce) {
			throw RuntimeError("Ring buffer size is too small for coalesce value {} < 2*{}",
				actualRingBdSize, std::max(readCoalesce, writeCoalesce));
		}
		setupScatterGather();
	}



	irqs[mm2sInterrupt].irqController->enableInterrupt(irqs[mm2sInterrupt], polling);
	irqs[s2mmInterrupt].irqController->enableInterrupt(irqs[s2mmInterrupt], polling);

	return true;
}

void Dma::setupScatterGather()
{
	setupScatterGatherRingRx();
	setupScatterGatherRingTx();
}

void Dma::setupScatterGatherRingRx()
{
	int ret;

	auto *rxRingPtr = XAxiDma_GetRxRing(&xDma);

	// Disable all RX interrupts before RxBD space setup
	XAxiDma_BdRingIntDisable(rxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	// Set delay and coalescing
	XAxiDma_BdRingSetCoalesce(rxRingPtr, readCoalesce, delay);

	// Allocate and map space for BD ring in host RAM
	auto &alloc = villas::HostRam::getAllocator();
	sgRingRx = alloc.allocateBlock(requestedRingBdSize * sizeof(uint16_t) * XAXIDMA_BD_NUM_WORDS);

	if (not card->mapMemoryBlock(*sgRingRx))
		throw RuntimeError("Memory not accessible by DMA");

	auto &mm = MemoryManager::get();
	auto trans = mm.getTranslation(busMasterInterfaces[sgInterface], sgRingRx->getAddrSpaceId());
	auto physAddr = reinterpret_cast<uintptr_t>(trans.getLocalAddr(0));
	auto virtAddr = reinterpret_cast<uintptr_t>(mm.getTranslationFromProcess(sgRingRx->getAddrSpaceId()).getLocalAddr(0));

	// Setup Rx BD space
	ret = XAxiDma_BdRingCreate(rxRingPtr, physAddr, virtAddr, XAXIDMA_BD_MINIMUM_ALIGNMENT, actualRingBdSize);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to create RX ring: {}", ret);

	// Setup an all-zero BD as the template for the Rx channel.
	XAxiDma_Bd bdTemplate;
	XAxiDma_BdClear(&bdTemplate);

	ret = XAxiDma_BdRingClone(rxRingPtr, &bdTemplate);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to clone BD template: {}", ret);

	// Enable completion interrupt
	XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
	// Start the RX channel
	ret = XAxiDma_BdRingStart(rxRingPtr);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to start TX ring: {}", ret);
}

void Dma::setupScatterGatherRingTx()
{
	int ret;

	auto *txRingPtr = XAxiDma_GetTxRing(&xDma);

	// Disable all TX interrupts before TxBD space setup
	XAxiDma_BdRingIntDisable(txRingPtr, XAXIDMA_IRQ_ALL_MASK);

	// Set TX delay and coalesce
	XAxiDma_BdRingSetCoalesce(txRingPtr, writeCoalesce, delay);

	// Allocate and map space for BD ring in host RAM
	auto &alloc = villas::HostRam::getAllocator();
	sgRingTx = alloc.allocateBlock(requestedRingBdSize * sizeof(uint16_t) * XAXIDMA_BD_NUM_WORDS);

	if (not card->mapMemoryBlock(*sgRingTx))
		throw RuntimeError("Memory not accessible by DMA");

	auto &mm = MemoryManager::get();
	auto trans = mm.getTranslation(busMasterInterfaces[sgInterface], sgRingTx->getAddrSpaceId());
	auto physAddr = reinterpret_cast<uintptr_t>(trans.getLocalAddr(0));
	auto virtAddr = reinterpret_cast<uintptr_t>(mm.getTranslationFromProcess(sgRingTx->getAddrSpaceId()).getLocalAddr(0));

	// Setup TxBD space
	ret = XAxiDma_BdRingCreate(txRingPtr, physAddr, virtAddr, XAXIDMA_BD_MINIMUM_ALIGNMENT, actualRingBdSize);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to create TX BD ring: {}", ret);

	// We create an all-zero BD as the template.
	XAxiDma_Bd BdTemplate;
	XAxiDma_BdClear(&BdTemplate);

	ret = XAxiDma_BdRingClone(txRingPtr, &BdTemplate);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to clone TX ring BD: {}", ret);

	// Enable completion interrupt
	XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	// Start the TX channel
	ret = XAxiDma_BdRingStart(txRingPtr);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to start TX ring: {}", ret);
}

bool Dma::reset()
{
	XAxiDma_IntrDisable(&xDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrDisable(&xDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
	XAxiDma_Reset(&xDma);

	// Value taken from libxil implementation
	int timeout = 500;

	while (timeout > 0) {
		if (XAxiDma_ResetIsDone(&xDma))
			return true;

		timeout--;
	}
	if (timeout == 0) {
		logger->error("DMA reset timed out");
	} else {
		logger->info("DMA has been reset.");
	}

	return false;
}

Dma::~Dma()
{
	reset();
}

bool Dma::memcpy(const MemoryBlock &src, const MemoryBlock &dst, size_t len)
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

bool Dma::write(const MemoryBlock &mem, size_t len)
{
	if (len == 0)
		return true;

	if (len > FPGA_DMA_BOUNDARY)
		return false;

	auto &mm = MemoryManager::get();

	// User has to make sure that memory is accessible, otherwise this will throw
	auto trans = mm.getTranslation(busMasterInterfaces[mm2sInterface], mem.getAddrSpaceId());
	const void *buf = reinterpret_cast<void *>(trans.getLocalAddr(0));
	if (buf == nullptr)
		throw RuntimeError("Buffer was null");

	logger->debug("Write to stream from address {:p}", buf);

	return hasScatterGather() ? writeScatterGather(buf, len) : writeSimple(buf, len);
}

bool Dma::read(const MemoryBlock &mem, size_t len)
{
	if (len == 0)
		return true;

	if (len > FPGA_DMA_BOUNDARY)
		return false;

	auto &mm = MemoryManager::get();

	// User has to make sure that memory is accessible, otherwise this will throw
	auto trans = mm.getTranslation(busMasterInterfaces[s2mmInterface], mem.getAddrSpaceId());
	void *buf = reinterpret_cast<void *>(trans.getLocalAddr(0));
	if (buf == nullptr)
		throw RuntimeError("Buffer was null");

	logger->debug("Read from stream and write to address {:p}", buf);

	return hasScatterGather() ? readScatterGather(buf, len) : readSimple(buf, len);
}

//Write a single message
bool Dma::writeScatterGather(const void *buf, size_t len)
{
	// buf is address from view of DMA controller

	int ret = XST_FAILURE;

	auto *txRing = XAxiDma_GetTxRing(&xDma);
	if (txRing == nullptr)
		throw RuntimeError("TxRing was null.");

	XAxiDma_Bd *bd;
	ret = XAxiDma_BdRingAlloc(txRing, 1, &bd);
	if (ret != XST_SUCCESS)
		throw RuntimeError("BdRingAlloc returned {}.", ret);

	ret = XAxiDma_BdSetBufAddr(bd, (uintptr_t) buf);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Setting BdBufAddr to {} returned {}.", buf, ret);

	ret = XAxiDma_BdSetLength(bd, len, txRing->MaxTransferLen);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Setting BdBufLength to {} returned {}.", len, ret);

	// We have a single descriptor so it is both start and end of the list
	XAxiDma_BdSetCtrl(bd, XAXIDMA_BD_CTRL_TXEOF_MASK | XAXIDMA_BD_CTRL_TXSOF_MASK);

	// TODO: Check if we really need this
	XAxiDma_BdSetId(bd, (uintptr_t) buf);

	// Give control of BD to HW. We should not access it until transfer is finished.
	// Failure could also indicate that EOF is not set on last Bd
	ret = XAxiDma_BdRingToHw(txRing, 1, bd);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Enqueuing Bd and giving control to HW failed {}", ret);

	return true;
}

bool Dma::readScatterGather(void *buf, size_t len)
{
	int ret = XST_FAILURE;

	if (len < readCoalesce*readMsgSize)
		throw RuntimeError("Read size is smaller than readCoalesce*msgSize. Cannot setup BDs.");

	auto *rxRing = XAxiDma_GetRxRing(&xDma);
	if (rxRing == nullptr)
		throw RuntimeError("RxRing was null.");

	XAxiDma_Bd *bd;
	ret = XAxiDma_BdRingAlloc(rxRing, readCoalesce, &bd);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to alloc BD in RX ring: {}", ret);

	auto curBd = bd;
	char* curBuf = (char*)buf;
	for (size_t i = 0; i < readCoalesce; i++) {
		ret = XAxiDma_BdSetBufAddr(curBd, (uintptr_t) curBuf);
		if (ret != XST_SUCCESS)
			throw RuntimeError("Failed to set buffer address {:x} on BD {:x}: {}", (uintptr_t) buf, (uintptr_t) bd, ret);

		ret = XAxiDma_BdSetLength(curBd, readMsgSize, rxRing->MaxTransferLen);
		if (ret != XST_SUCCESS)
			throw RuntimeError("Rx set length {} on BD {:x} failed {}", len, (uintptr_t) bd, ret);


		// Receive BDs do not need to set anything for the control
		// The hardware will set the SOF/EOF bits per stream status
		XAxiDma_BdSetCtrl(curBd, 0);

		// TODO: Check if we really need this
		XAxiDma_BdSetId(curBd, (uintptr_t) buf);

		curBuf += readMsgSize;
		curBd = (XAxiDma_Bd *)XAxiDma_BdRingNext(rxRing, curBd);
	}

	ret = XAxiDma_BdRingToHw(rxRing, readCoalesce, bd);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to submit BD to RX ring: {}", ret);

	return true;
}

size_t
Dma::writeCompleteScatterGather()
{
	XAxiDma_Bd *bd = nullptr, *curBd;
	size_t processedBds = 0;
	auto txRing = XAxiDma_GetTxRing(&xDma);
	int ret = XST_FAILURE;
	size_t bytesWritten = 0;

	if ((processedBds = XAxiDma_BdRingFromHw(txRing, 1, &bd)) == 0)
	{
		/*auto intrNum = */irqs[mm2sInterrupt].irqController->waitForInterrupt(irqs[mm2sInterrupt].num);
		//logger->info("Got {} interrupts (id: {}) from write channel", intrNum, irqs[mm2sInterrupt].num);
		processedBds = XAxiDma_BdRingFromHw(txRing, 1, &bd);
	}

	// Acknowledge the interrupt
	auto irqStatus = XAxiDma_BdRingGetIrq(txRing);
	XAxiDma_BdRingAckIrq(txRing, irqStatus);

	if (bd == nullptr)
		throw RuntimeError("Bd was null.");

	curBd = bd;
	for (size_t i = 0; i < processedBds; i++) {
		ret = XAxiDma_BdGetSts(curBd);
		if ((ret & XAXIDMA_BD_STS_ALL_ERR_MASK) || (!(ret & XAXIDMA_BD_STS_COMPLETE_MASK))) {
			throw RuntimeError("Bd Status register shows error: {}", ret);
			break;
		}

		bytesWritten += XAxiDma_BdGetLength(bd, txRing->MaxTransferLen);
		curBd = (XAxiDma_Bd *) XAxiDma_BdRingNext(txRing, curBd);
	}

	ret = XAxiDma_BdRingFree(txRing, processedBds, bd);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to free {} TX BDs {}", processedBds, ret);

	return bytesWritten;
}

size_t
Dma::readCompleteScatterGather()
{
	XAxiDma_Bd *bd = nullptr, *curBd;
	size_t processedBds = 0;
	auto rxRing = XAxiDma_GetRxRing(&xDma);
	int ret = XST_FAILURE;
	size_t bytesRead = 0;

	// Wait until the data has been received by the RX channel.
	if ((processedBds = XAxiDma_BdRingFromHw(rxRing, readCoalesce, &bd)) < readCoalesce)
	{
		if (processedBds != 0) {
			//Ignore partial batches
			logger->warn("Ignoring partial batch of {} BDs.", processedBds);
			ret = XAxiDma_BdRingFree(rxRing, processedBds, bd);
			if (ret != XST_SUCCESS)
				throw RuntimeError("Failed to free {} RX BDs {}", processedBds, ret);
		}
		//auto intrNum =
		irqs[s2mmInterrupt].irqController->waitForInterrupt(irqs[s2mmInterrupt].num);
		//If we got a partial batch on the first call, we have to receive up to readCoalesce*2
		//to make sure we get a full batch of readCoalesce messages
		processedBds = XAxiDma_BdRingFromHw(rxRing, readCoalesce*2, &bd);
	}
	if(processedBds < readCoalesce) {
		// We got less than we expected. We already tried two times so let's give up.
		throw RuntimeError("Read only {} BDs, expected {}.", processedBds, readCoalesce);
	} else if(processedBds > readCoalesce) {
		// If the first try was a partial batch, we receive two batches on the second try
		// We ignore the first batch and only process the second one
		while (processedBds > readCoalesce) {
			bd = (XAxiDma_Bd *) XAxiDma_BdRingNext(rxRing, bd);
			processedBds--;
		}
		ret = XAxiDma_BdRingFree(rxRing, processedBds-readCoalesce, bd);
		if (ret != XST_SUCCESS)
			throw RuntimeError("Failed to free {} RX BDs {}", processedBds, ret);
	}
	// At this point we have exactly readCoalesce BDs.

	// Acknowledge the interrupt. Has no effect if no interrupt has occured.
	auto irqStatus = XAxiDma_BdRingGetIrq(rxRing);
	XAxiDma_BdRingAckIrq(rxRing, irqStatus);

	if (bd == nullptr)
		throw RuntimeError("Bd was null.");

	curBd = bd;
	for (size_t i = 0; i < processedBds; i++) {
		ret = XAxiDma_BdGetSts(curBd);
		if ((ret & XAXIDMA_BD_STS_ALL_ERR_MASK) || (!(ret & XAXIDMA_BD_STS_COMPLETE_MASK))) {
			throw RuntimeError("Bd Status register shows error: {}", ret);
			break;
		}

		bytesRead += XAxiDma_BdGetActualLength(bd, rxRing->MaxTransferLen);
		curBd = (XAxiDma_Bd *) XAxiDma_BdRingNext(rxRing, curBd);
	}

	// Free all processed RX BDs for future transmission.
	ret = XAxiDma_BdRingFree(rxRing, processedBds, bd);
	if (ret != XST_SUCCESS)
		throw RuntimeError("Failed to free {} TX BDs {}.", processedBds, ret);

	return bytesRead;
}

bool Dma::writeSimple(const void *buf, size_t len)
{
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(&xDma);

	if (not ring->HasDRE) {
		const uint32_t mask = xDma.MicroDmaMode
			? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
			: ring->DataWidth - 1;

		if (reinterpret_cast<uintptr_t>(buf) & mask)
			return false;
	}

	const bool dmaChannelHalted =
		XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;

	const bool dmaToDeviceBusy = XAxiDma_Busy(&xDma, XAXIDMA_DMA_TO_DEVICE);

	// If the engine is doing a transfer, cannot submit
	if (not dmaChannelHalted and dmaToDeviceBusy)
		return false;

	// Set lower 32 bit of source address
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_OFFSET,
					 LOWER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// If neccessary, set upper 32 bit of source address
	if (xDma.AddrWidth > 32)
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_MSB_OFFSET,
						 UPPER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// Start DMA channel
	auto channelControl = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET);
	channelControl |= XAXIDMA_CR_RUNSTOP_MASK;
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_CR_OFFSET, channelControl);

	// Set tail descriptor pointer
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET, len);

	return true;
}

bool Dma::readSimple(void *buf, size_t len)
{
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(&xDma);

	if (not ring->HasDRE) {
		const uint32_t mask = xDma.MicroDmaMode
			? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
			: ring->DataWidth - 1;

		if (reinterpret_cast<uintptr_t>(buf) & mask)
			return false;
	}

	const bool dmaChannelHalted = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;
	const bool deviceToDmaBusy = XAxiDma_Busy(&xDma, XAXIDMA_DEVICE_TO_DMA);

	// If the engine is doing a transfer, cannot submit
	if (not dmaChannelHalted and deviceToDmaBusy)
		return false;

	// Set lower 32 bit of destination address
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_OFFSET, LOWER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

	// If neccessary, set upper 32 bit of destination address
	if (xDma.AddrWidth > 32)
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_MSB_OFFSET, UPPER_32_BITS(reinterpret_cast<uintptr_t>(buf)));

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

	const XAxiDma_BdRing *ring = XAxiDma_GetTxRing(&xDma);
	const size_t bytesWritten = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);

	return bytesWritten;
}

size_t
Dma::readCompleteSimple()
{
	while (!(XAxiDma_IntrGetIrq(&xDma, XAXIDMA_DEVICE_TO_DMA) & XAXIDMA_IRQ_IOC_MASK))
		irqs[s2mmInterrupt].irqController->waitForInterrupt(irqs[s2mmInterrupt]);

	XAxiDma_IntrAckIrq(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	const XAxiDma_BdRing *ring = XAxiDma_GetRxRing(&xDma);
	const size_t bytesRead = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);

	return bytesRead;
}

void Dma::makeAccesibleFromVA(const MemoryBlock &mem)
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

bool Dma::isMemoryBlockAccesible(const MemoryBlock &mem, const std::string &interface)
{
	auto &mm = MemoryManager::get();

	try {
		mm.findPath(getMasterAddrSpaceByInterface(interface), mem.getAddrSpaceId());
	}
	catch (const std::out_of_range &) {
		return false; // Not (yet) accessible
	}

	return true;
}

void Dma::dump()
{
	Core::dump();

	logger->info("S2MM_DMACR:  {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_CR_OFFSET));
	logger->info("S2MM_DMASR:  {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_SR_OFFSET));

	if (!hasScatterGather())
		logger->info("S2MM_LENGTH: {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_BUFFLEN_OFFSET));

	logger->info("MM2S_DMACR:  {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_TX_OFFSET + XAXIDMA_CR_OFFSET));
	logger->info("MM2S_DMASR:  {:x}", XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_TX_OFFSET + XAXIDMA_SR_OFFSET));
}

void DmaFactory::configure(Core &ip, json_t *cfg)
{
	NodeFactory::configure(ip, cfg);

	auto &dma = dynamic_cast<Dma&>(ip);

	// Sensible default configuration
	dma.xConfig.HasStsCntrlStrm = 0;
	dma.xConfig.HasMm2S = 1;
	dma.xConfig.HasMm2SDRE = 0;
	dma.xConfig.Mm2SDataWidth = 32;
	dma.xConfig.HasS2Mm = 1;
	dma.xConfig.HasS2MmDRE = 0;
	dma.xConfig.HasSg = 1;
	dma.xConfig.S2MmDataWidth = 32;
	dma.xConfig.Mm2sNumChannels = 1;
	dma.xConfig.S2MmNumChannels = 1;
	dma.xConfig.Mm2SBurstSize = 16;
	dma.xConfig.S2MmBurstSize = 16;
	dma.xConfig.MicroDmaMode = 0;
	dma.xConfig.AddrWidth = 32;
	dma.xConfig.SgLengthWidth = 14;

	json_error_t err;
	int ret = json_unpack_ex(cfg, &err, 0, "{ s: { s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i } }",
		"parameters",
			"c_sg_include_stscntrl_strm", 	&dma.xConfig.HasStsCntrlStrm,
			"c_include_mm2s", 		&dma.xConfig.HasMm2S,
			"c_include_mm2s_dre", 		&dma.xConfig.HasMm2SDRE,
			"c_m_axi_mm2s_data_width", 	&dma.xConfig.Mm2SDataWidth,
			"c_include_s2mm", 		&dma.xConfig.HasS2Mm,
			"c_include_s2mm_dre", 		&dma.xConfig.HasS2MmDRE,
			"c_m_axi_s2mm_data_width", 	&dma.xConfig.S2MmDataWidth,
			"c_include_sg", 		&dma.xConfig.HasSg,
			"c_num_mm2s_channels", 		&dma.xConfig.Mm2sNumChannels,
			"c_num_s2mm_channels", 		&dma.xConfig.S2MmNumChannels,
			"c_micro_dma", 			&dma.xConfig.MicroDmaMode,
			"c_addr_width", 		&dma.xConfig.AddrWidth,
			"c_sg_length_width", 		&dma.xConfig.SgLengthWidth
	);
	if (ret != 0)
		throw ConfigError(cfg, err, "", "Failed to parse DMA configuration");
}
