/* DMA driver
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2018-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sstream>
#include <string>
#include <sys/types.h>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/intc.hpp>
#include <villas/memory.hpp>

#include <xilinx/xaxidma.h>
#include <xilinx/xaxidma_bd.h>
#include <xilinx/xaxidma_hw.h>

// Max. size of a DMA transfer in simple mode
#define FPGA_DMA_BOUNDARY 0x1000

using namespace villas::fpga::ip;

bool Dma::init() {
  // Check if configJson has been called
  if (!configDone)
    throw RuntimeError("DMA device not configured");

  if (hasScatterGather())
    logger->info("Scatter-Gather support: {}", hasScatterGather());

  xConfig.BaseAddr = getBaseAddr(registerMemory);

  hwReadLock.lock();
  hwWriteLock.lock();
  if (XAxiDma_CfgInitialize(&xDma, &xConfig) != XST_SUCCESS) {
    logger->error("Cannot initialize Xilinx DMA driver");
    return false;
  }

  if (XAxiDma_Selftest(&xDma) != XST_SUCCESS) {
    logger->error("DMA selftest failed");
    return false;
  } else {
    logger->debug("DMA selftest passed");
  }

  hwWriteLock.unlock();
  hwReadLock.unlock();
  // Map buffer descriptors
  if (hasScatterGather()) {
    if (actualRingBdSize < readCoalesce || actualRingBdSize < writeCoalesce) {
      throw RuntimeError(
          "Ring buffer size is too small for coalesce value {} < {}",
          actualRingBdSize, std::max(readCoalesce, writeCoalesce));
    }
    setupScatterGather();
  }

  if (!polling) {
    irqs[mm2sInterrupt].irqController->enableInterrupt(irqs[mm2sInterrupt],
                                                       polling);
    irqs[s2mmInterrupt].irqController->enableInterrupt(irqs[s2mmInterrupt],
                                                       polling);
  }
  return true;
}

void Dma::setupScatterGather() {
  // Allocate and map space for BD ring in host RAM
  auto &alloc = villas::HostRam::getAllocator();
  sgRing = alloc.allocateBlock(2 * requestedRingBdSizeMemory);

  if (not card->mapMemoryBlock(sgRing))
    throw RuntimeError("Memory not accessible by DMA");

  auto &mm = MemoryManager::get();
  auto trans = mm.getTranslation(busMasterInterfaces[sgInterface],
                                 sgRing->getAddrSpaceId());

  auto physAddr = reinterpret_cast<uintptr_t>(trans.getLocalAddr(0));
  auto virtAddr = reinterpret_cast<uintptr_t>(
      mm.getTranslationFromProcess(sgRing->getAddrSpaceId()).getLocalAddr(0));
  setupScatterGatherRingRx(physAddr, virtAddr);

  setupScatterGatherRingTx(physAddr + requestedRingBdSizeMemory,
                           virtAddr + requestedRingBdSizeMemory);
}

void Dma::setupScatterGatherRingRx(uintptr_t physAddr, uintptr_t virtAddr) {
  int ret;

  hwReadLock.lock();
  auto *rxRingPtr = XAxiDma_GetRxRing(&xDma);

  // Disable all RX interrupts before RxBD space setup
  XAxiDma_BdRingIntDisable(rxRingPtr, XAXIDMA_IRQ_ALL_MASK);

  // Set delay and coalescing
  XAxiDma_BdRingSetCoalesce(rxRingPtr, readCoalesce, delay);

  // Setup Rx BD space
  ret = XAxiDma_BdRingCreate(rxRingPtr, physAddr, virtAddr,
                             XAXIDMA_BD_MINIMUM_ALIGNMENT, actualRingBdSize);
  if (ret != XST_SUCCESS)
    throw RuntimeError("Failed to create RX ring: {}", ret);

  // Setup an all-zero BD as the template for the Rx channel.
  XAxiDma_Bd bdTemplate;
  XAxiDma_BdClear(&bdTemplate);

  ret = XAxiDma_BdRingClone(rxRingPtr, &bdTemplate);
  if (ret != XST_SUCCESS)
    throw RuntimeError("Failed to clone BD template: {}", ret);

  if (cyclic) {
    // Enable Cyclic DMA mode
    XAxiDma_BdRingEnableCyclicDMA(rxRingPtr);
    XAxiDma_SelectCyclicMode(&xDma, XAXIDMA_DEVICE_TO_DMA, 1);
  }
  // Enable completion interrupt
  if (!polling) {
    XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
  }
  // Start the RX channel
  ret = XAxiDma_BdRingStart(rxRingPtr);
  if (ret != XST_SUCCESS)
    throw RuntimeError("Failed to start TX ring: {}", ret);

  hwReadLock.unlock();
}

void Dma::setupScatterGatherRingTx(uintptr_t physAddr, uintptr_t virtAddr) {
  int ret;

  hwWriteLock.lock();
  auto *txRingPtr = XAxiDma_GetTxRing(&xDma);

  // Disable all TX interrupts before TxBD space setup
  XAxiDma_BdRingIntDisable(txRingPtr, XAXIDMA_IRQ_ALL_MASK);

  // Set TX delay and coalesce
  XAxiDma_BdRingSetCoalesce(txRingPtr, writeCoalesce, delay);

  // Setup TxBD space
  ret = XAxiDma_BdRingCreate(txRingPtr, physAddr, virtAddr,
                             XAXIDMA_BD_MINIMUM_ALIGNMENT, actualRingBdSize);
  if (ret != XST_SUCCESS)
    throw RuntimeError("Failed to create TX BD ring: {}", ret);

  // We create an all-zero BD as the template.
  XAxiDma_Bd BdTemplate;
  XAxiDma_BdClear(&BdTemplate);

  ret = XAxiDma_BdRingClone(txRingPtr, &BdTemplate);
  if (ret != XST_SUCCESS)
    throw RuntimeError("Failed to clone TX ring BD: {}", ret);

  // Enable completion interrupt
  if (!polling) {
    XAxiDma_IntrEnable(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
  }
  // Start the TX channel
  ret = XAxiDma_BdRingStart(txRingPtr);
  if (ret != XST_SUCCESS)
    throw RuntimeError("Failed to start TX ring: {}", ret);
  hwWriteLock.unlock();
}

bool Dma::reset() {
  if (xDma.RegBase == 0) {
    return true;
  }

  hwReadLock.lock();
  hwWriteLock.lock();
  XAxiDma_IntrDisable(&xDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
  XAxiDma_IntrDisable(&xDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

  XAxiDma_Reset(&xDma);

  // Value taken from libxil implementation
  int timeout = 500;

  while (timeout > 0) {
    if (XAxiDma_ResetIsDone(&xDma)) {
      logger->info("DMA has been reset.");
      hwReadLock.unlock();
      hwWriteLock.unlock();
      return true;
    }

    timeout--;
  }
  hwReadLock.unlock();
  hwWriteLock.unlock();
  logger->error("DMA reset timed out");

  return false;
}

Dma::~Dma() {
  // Unmap memory in our ownership, MemoryBlock gets deleted and removed from
  // graph by this destructor as well.
  if (hasScatterGather()) {
    // Fix memory leak in upstream Xilinx code
    auto txRingPtr = XAxiDma_GetTxRing(&xDma);
    auto rxRingPtr = XAxiDma_GetRxRing(&xDma);
    if (txRingPtr) {
      free(txRingPtr->CyclicBd);
      txRingPtr->CyclicBd = nullptr;
    }
    if (rxRingPtr) {
      free(rxRingPtr->CyclicBd);
      rxRingPtr->CyclicBd = nullptr;
    }
    // Unmap SG memory Blocks
    if (sgRing) {
      card->unmapMemoryBlock(*sgRing);
    }
  }
  Dma::reset();
}

bool Dma::memcpy(const MemoryBlock &src, const MemoryBlock &dst, size_t len) {
  if (len == 0)
    return true;

  if (not connectLoopback())
    return false;

  if (this->read(dst, len) == 0)
    return false;

  if (this->write(src, len) == 0)
    return false;

  if (not this->writeComplete().bds)
    return false;

  if (not this->readComplete().bds)
    return false;

  return true;
}

bool Dma::write(const MemoryBlock &mem, size_t len) {
  if (len == 0)
    return true;

  if (len > FPGA_DMA_BOUNDARY)
    return false;

  auto &mm = MemoryManager::get();

  // User has to make sure that memory is accessible, otherwise this will throw
  auto trans = mm.getTranslation(busMasterInterfaces[mm2sInterface],
                                 mem.getAddrSpaceId());
  const void *buf = reinterpret_cast<void *>(trans.getLocalAddr(0));
  if (buf == nullptr)
    throw RuntimeError("Buffer was null");

  logger->trace("Write to stream from address {:p}", buf);

  return hasScatterGather() ? writeScatterGather(buf, len)
                            : writeSimple(buf, len);
}

bool Dma::read(const MemoryBlock &mem, size_t len) {
  if (len == 0)
    return true;

  if (len > FPGA_DMA_BOUNDARY)
    return false;

  auto &mm = MemoryManager::get();

  // User has to make sure that memory is accessible, otherwise this will throw
  auto trans = mm.getTranslation(busMasterInterfaces[s2mmInterface],
                                 mem.getAddrSpaceId());
  void *buf = reinterpret_cast<void *>(trans.getLocalAddr(0));
  if (buf == nullptr)
    throw RuntimeError("Buffer was null");

  return hasScatterGather() ? readScatterGather(buf, len)
                            : readSimple(buf, len);
}

// Reuse existing single BD bypassing BdRingFree, Alloc, ToHw
bool Dma::writeScatterGatherFast() {
  hwWriteLock.lock();
  auto *txRing = XAxiDma_GetTxRing(&xDma);
  if (txRing == nullptr) {
    hwWriteLock.unlock();
    throw RuntimeError("RxRing was null.");
  }
  XAxiDma_Bd *CurBdPtr = txRing->HwHead;

  // Clear the bit we are polling on in complete
  uint32_t BdSts = XAxiDma_ReadReg((UINTPTR)CurBdPtr, XAXIDMA_BD_STS_OFFSET);
  BdSts &= ~XAXIDMA_BD_STS_COMPLETE_MASK;
  XAxiDma_BdWrite(CurBdPtr, XAXIDMA_BD_STS_OFFSET, BdSts);

  uintptr_t tdesc = ((uintptr_t)txRing->HwTail +
                     (txRing->FirstBdPhysAddr - txRing->FirstBdAddr)) &
                    XAXIDMA_DESC_LSB_MASK;
  XAxiDma_WriteReg(txRing->ChanBase, XAXIDMA_TDESC_OFFSET, tdesc);

  hwWriteLock.unlock();
  return true;
}

bool Dma::writeScatterGatherPrepare(const MemoryBlock &mem, size_t len) {

  auto &mm = MemoryManager::get();

  // User has to make sure that memory is accessible, otherwise this will throw
  auto trans = mm.getTranslation(busMasterInterfaces[mm2sInterface],
                                 mem.getAddrSpaceId());
  void *buf = reinterpret_cast<void *>(trans.getLocalAddr(0));
  if (buf == nullptr) {
    throw RuntimeError("Buffer was null");
  }
  hwWriteLock.lock();

  auto bd = writeScatterGatherSetupBd(buf, len);

  hwWriteLock.unlock();

  return bd != nullptr;
}

XAxiDma_Bd *Dma::writeScatterGatherSetupBd(const void *buf, size_t len) {
  uint32_t ret = XST_FAILURE;
  if (len == 0)
    return nullptr;

  if (len > FPGA_DMA_BOUNDARY)
    return nullptr;

  auto *txRing = XAxiDma_GetTxRing(&xDma);
  if (txRing == nullptr) {
    hwWriteLock.unlock();
    throw RuntimeError("TxRing was null.");
  }

  XAxiDma_Bd *bd;
  ret = XAxiDma_BdRingAlloc(txRing, 1, &bd);
  if (ret != XST_SUCCESS) {
    hwWriteLock.unlock();
    throw RuntimeError("Write: BdRingAlloc returned {}.", ret);
  }

  ret = XAxiDma_BdSetBufAddr(bd, (uintptr_t)buf);
  if (ret != XST_SUCCESS) {
    hwWriteLock.unlock();
    throw RuntimeError("Setting BdBufAddr to {} returned {}.", buf, ret);
  }

  ret = XAxiDma_BdSetLength(bd, len, txRing->MaxTransferLen);
  if (ret != XST_SUCCESS) {
    hwWriteLock.unlock();
    throw RuntimeError("Setting BdBufLength to {} returned {}.", len, ret);
  }

  // We have a single descriptor so it is both start and end of the list
  XAxiDma_BdSetCtrl(bd,
                    XAXIDMA_BD_CTRL_TXEOF_MASK | XAXIDMA_BD_CTRL_TXSOF_MASK);

  // TODO: Check if we really need this
  XAxiDma_BdSetId(bd, (uintptr_t)buf);
  return bd;
}

// Write a single message
bool Dma::writeScatterGather(const void *buf, size_t len) {
  // buf is address from view of DMA controller

  int ret = XST_FAILURE;
  hwWriteLock.lock();
  auto *txRing = XAxiDma_GetTxRing(&xDma);
  if (txRing == nullptr) {
    hwWriteLock.unlock();
    throw RuntimeError("TxRing was null.");
  }

  XAxiDma_Bd *bd = writeScatterGatherSetupBd(buf, len);
  // Give control of BD to HW. We should not access it until transfer is finished.
  // Failure could also indicate that EOF is not set on last Bd
  ret = XAxiDma_BdRingToHw(txRing, 1, bd);
  if (ret != XST_SUCCESS) {
    hwWriteLock.unlock();
    throw RuntimeError("Enqueuing Bd and giving control to HW failed {}", ret);
  }

  hwWriteLock.unlock();

  return true;
}

// Reuse existing single BD bypassing BdRingFree, Alloc, ToHw
bool Dma::readScatterGatherFast() {
  hwReadLock.lock();
  auto *rxRing = XAxiDma_GetRxRing(&xDma);
  if (rxRing == nullptr) {
    hwReadLock.unlock();
    throw RuntimeError("RxRing was null.");
  }
  XAxiDma_Bd *CurBdPtr = rxRing->HwHead;
  // Poll BD status to avoid accessing PCIe address space
  uint32_t BdSts = XAxiDma_ReadReg((UINTPTR)CurBdPtr, XAXIDMA_BD_STS_OFFSET);

  // Clear the bit we are polling on in complete
  BdSts &= ~XAXIDMA_BD_STS_COMPLETE_MASK;
  XAxiDma_BdWrite(CurBdPtr, XAXIDMA_BD_STS_OFFSET, BdSts);

  uintptr_t tdesc = ((uintptr_t)rxRing->HwTail +
                     (rxRing->FirstBdPhysAddr - rxRing->FirstBdAddr)) &
                    XAXIDMA_DESC_LSB_MASK;
  XAxiDma_WriteReg(rxRing->ChanBase, XAXIDMA_TDESC_OFFSET, tdesc);

  hwReadLock.unlock();
  return true;
}

bool Dma::readScatterGatherPrepare(const MemoryBlock &mem, size_t len) {

  auto &mm = MemoryManager::get();

  // User has to make sure that memory is accessible, otherwise this will throw
  auto trans = mm.getTranslation(busMasterInterfaces[s2mmInterface],
                                 mem.getAddrSpaceId());
  void *buf = reinterpret_cast<void *>(trans.getLocalAddr(0));
  if (buf == nullptr) {
    throw RuntimeError("Buffer was null");
  }
  hwReadLock.lock();

  auto bd = readScatterGatherSetupBd(buf, len);

  hwReadLock.unlock();

  return bd != nullptr;
}

XAxiDma_Bd *Dma::readScatterGatherSetupBd(void *buf, size_t len) {
  uint32_t ret = XST_FAILURE;
  if (len == 0)
    return nullptr;

  if (len > FPGA_DMA_BOUNDARY)
    return nullptr;

  auto *rxRing = XAxiDma_GetRxRing(&xDma);
  if (rxRing == nullptr) {
    hwReadLock.unlock();
    throw RuntimeError("RxRing was null.");
  }

  XAxiDma_Bd *bd;
  ret = XAxiDma_BdRingAlloc(rxRing, readCoalesce, &bd);
  if (ret != XST_SUCCESS) {
    hwReadLock.unlock();
    throw RuntimeError("Failed to alloc BD in RX ring: {}", ret);
  }

  auto curBd = bd;
  char *curBuf = (char *)buf;
  for (size_t i = 0; i < readCoalesce; i++) {
    ret = XAxiDma_BdSetBufAddr(curBd, (uintptr_t)curBuf);
    if (ret != XST_SUCCESS) {
      hwReadLock.unlock();
      throw RuntimeError("Failed to set buffer address {:x} on BD {:x}: {}",
                         (uintptr_t)buf, (uintptr_t)bd, ret);
    }

    ret = XAxiDma_BdSetLength(curBd, readMsgSize, rxRing->MaxTransferLen);
    if (ret != XST_SUCCESS) {
      hwReadLock.unlock();
      throw RuntimeError("Rx set length {} on BD {:x} failed {}", len,
                         (uintptr_t)bd, ret);
    }

    // Receive BDs do not need to set anything for the control
    // The hardware will set the SOF/EOF bits per stream status
    XAxiDma_BdSetCtrl(curBd, 0);

    // TODO: Check if we really need this
    XAxiDma_BdSetId(curBd, (uintptr_t)buf);

    curBuf += readMsgSize;
    curBd = (XAxiDma_Bd *)XAxiDma_BdRingNext(rxRing, curBd);
  }
  return bd;
}

bool Dma::readScatterGather(void *buf, size_t len) {
  uint32_t ret = XST_FAILURE;

  if (len < readCoalesce * readMsgSize) {
    throw RuntimeError(
        "Read size is smaller than readCoalesce*msgSize. Cannot setup BDs.");
  }

  hwReadLock.lock();
  auto *rxRing = XAxiDma_GetRxRing(&xDma);
  if (rxRing == nullptr) {
    hwReadLock.unlock();
    throw RuntimeError("RxRing was null.");
  }

  XAxiDma_Bd *bd = readScatterGatherSetupBd(buf, len);

  ret = XAxiDma_BdRingToHw(rxRing, readCoalesce, bd);
  if (ret != XST_SUCCESS) {
    hwReadLock.unlock();
    throw RuntimeError("Failed to submit BD to RX ring: {}", ret);
  }

  hwReadLock.unlock();
  return true;
}

size_t Dma::writeScatterGatherPoll(bool lock) {
  if (lock) {
    hwWriteLock.lock();
  }
  auto txRing = XAxiDma_GetTxRing(&xDma);
  XAxiDma_Bd *CurBdPtr = txRing->HwHead;
  volatile uint32_t BdSts;
  // Poll BD status to avoid accessing PCIe address space
  do {
    BdSts = XAxiDma_ReadReg((UINTPTR)CurBdPtr, XAXIDMA_BD_STS_OFFSET);
  } while (!(BdSts & XAXIDMA_BD_STS_COMPLETE_MASK));
  // At this point, we know that the transmission is complete, but we haven't accessed the
  // PCIe address space, yet. We know that we have received at least one BD.
  if (lock) {
    hwWriteLock.unlock();
  }
  return XAxiDma_BdGetActualLength(CurBdPtr, XAXIDMA_MCHAN_MAX_TRANSFER_LEN);
}

Dma::Completion Dma::writeCompleteScatterGather() {
  Completion c;
  XAxiDma_Bd *bd = nullptr, *curBd;
  auto txRing = XAxiDma_GetTxRing(&xDma);
  uint32_t ret = XST_FAILURE;
  static size_t errcnt = 32;

  uint32_t irqStatus = 0;
  if (polling) {
    hwWriteLock.lock();
    writeScatterGatherPoll(false);
  } else {
    c.interrupts = irqs[mm2sInterrupt].irqController->waitForInterrupt(
        irqs[mm2sInterrupt].num);
    hwWriteLock.lock();
  }

  if ((c.bds = XAxiDma_BdRingFromHw(txRing, writeCoalesce, &bd)) <
      writeCoalesce) {
    logger->warn("Send partial batch of {}/{} BDs.", c.bds, writeCoalesce);
    if (errcnt-- == 0) {
      hwWriteLock.unlock();
      throw RuntimeError("too many partial batches");
    }
  }

  // Acknowledge the interrupt
  if (!polling) {
    irqStatus = XAxiDma_BdRingGetIrq(txRing);
    XAxiDma_BdRingAckIrq(txRing, irqStatus);
  }

  if (c.bds == 0) {
    c.bytes = 0;
    hwWriteLock.unlock();
    return c;
  }

  if (bd == nullptr) {
    hwWriteLock.unlock();
    throw RuntimeError("Bd was null.");
  }

  curBd = bd;
  for (size_t i = 0; i < c.bds; i++) {
    ret = XAxiDma_BdGetSts(curBd);
    if ((ret & XAXIDMA_BD_STS_ALL_ERR_MASK) ||
        (!(ret & XAXIDMA_BD_STS_COMPLETE_MASK))) {
      hwWriteLock.unlock();
      throw RuntimeError("Write: Bd Status register shows error: {:#x}", ret);
    }

    c.bytes += XAxiDma_BdGetLength(bd, txRing->MaxTransferLen);
    curBd = (XAxiDma_Bd *)XAxiDma_BdRingNext(txRing, curBd);
  }

  ret = XAxiDma_BdRingFree(txRing, c.bds, bd);
  if (ret != XST_SUCCESS) {
    hwWriteLock.unlock();
    throw RuntimeError("Failed to free {} TX BDs {}", c.bds, ret);
  }

  hwWriteLock.unlock();
  return c;
}

size_t Dma::readScatterGatherPoll(bool lock) {
  if (lock) {
    hwReadLock.lock();
  }
  auto rxRing = XAxiDma_GetRxRing(&xDma);
  XAxiDma_Bd *CurBdPtr = rxRing->HwHead;
  volatile uint32_t BdSts;
  // Poll BD status to avoid accessing PCIe address space
  do {
    BdSts = XAxiDma_ReadReg((UINTPTR)CurBdPtr, XAXIDMA_BD_STS_OFFSET);
  } while (!(BdSts & XAXIDMA_BD_STS_COMPLETE_MASK));
  // At this point, we know that the transmission is complete, but we haven't accessed the
  // PCIe address space, yet. We know that we have received at least one BD.
  if (lock) {
    hwReadLock.unlock();
  }
  return XAxiDma_BdGetActualLength(CurBdPtr, XAXIDMA_MCHAN_MAX_TRANSFER_LEN);
}

Dma::Completion Dma::readCompleteScatterGather() {
  Completion c;
  XAxiDma_Bd *bd = nullptr, *curBd;
  auto rxRing = XAxiDma_GetRxRing(&xDma);
  int ret = XST_FAILURE;
  static size_t errcnt = 32;

  ssize_t intrs = 0;
  uint32_t irqStatus = 0;
  if (polling) {
    hwReadLock.lock();
    readScatterGatherPoll(false);
    intrs = 1;
  } else {
    intrs = irqs[s2mmInterrupt].irqController->waitForInterrupt(
        irqs[s2mmInterrupt].num);
    hwReadLock.lock();
  }

  if (intrs < 0) {
    logger->warn("Interrupt error or timeout: {}", intrs);

    // Free all RX BDs for future transmission.
    int bds = XAxiDma_BdRingFromHw(rxRing, XAXIDMA_ALL_BDS, &bd);
    XAxiDma_BdRingFree(rxRing, bds, bd);
    hwReadLock.unlock();

    c.interrupts = 0;
    return c;
  } else {
    c.interrupts = intrs;
  }
  if (!polling) {
    irqStatus = XAxiDma_BdRingGetIrq(rxRing);
    XAxiDma_BdRingAckIrq(rxRing, irqStatus);
    if (!(irqStatus & XAXIDMA_IRQ_IOC_MASK)) {
      logger->error("Expected IOC interrupt but IRQ status is: {:#x}",
                    irqStatus);
      return c;
    }
  }
  // Wait until the data has been received by the RX channel.
  if ((c.bds = XAxiDma_BdRingFromHw(rxRing, readCoalesce, &bd)) <
      readCoalesce) {
    logger->warn("Got partial batch of {}/{} BDs.", c.bds, readCoalesce);
    if (errcnt-- == 0) {
      hwReadLock.unlock();
      throw RuntimeError("too many partial batches");
    }
  } else {
    errcnt = 32;
  }

  if (c.bds == 0) {
    c.bytes = 0;
    hwReadLock.unlock();
    return c;
  }

  if (bd == nullptr) {
    hwReadLock.unlock();
    throw RuntimeError("Bd was null.");
  }

  curBd = bd;
  for (size_t i = 0; i < c.bds; i++) {
    // logger->trace("Read BD {}/{}: {:#x}", i, c.bds,
    //               XAxiDma_BdGetBufAddr(curBd));
    ret = XAxiDma_BdGetSts(curBd);
    if ((ret & XAXIDMA_BD_STS_ALL_ERR_MASK) ||
        (!(ret & XAXIDMA_BD_STS_COMPLETE_MASK))) {
      hwReadLock.unlock();
      throw RuntimeError("Read: Bd Status register shows error: {}", ret);
    }

    c.bytes += XAxiDma_BdGetActualLength(bd, rxRing->MaxTransferLen);
    curBd = (XAxiDma_Bd *)XAxiDma_BdRingNext(rxRing, curBd);
  }

  // Free all processed RX BDs for future transmission.
  ret = XAxiDma_BdRingFree(rxRing, c.bds, bd);
  if (ret != XST_SUCCESS) {
    hwReadLock.unlock();
    throw RuntimeError("Failed to free {} TX BDs {}.", c.bds, ret);
  }

  hwReadLock.unlock();
  return c;
}

bool Dma::writeSimple(const void *buf, size_t len) {
  hwWriteLock.lock();
  XAxiDma_BdRing *ring = XAxiDma_GetTxRing(&xDma);

  if (not ring->HasDRE) {
    const uint32_t mask = xDma.MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
                                            : ring->DataWidth - 1;

    if (reinterpret_cast<uintptr_t>(buf) & mask) {
      hwWriteLock.unlock();
      return false;
    }
  }

  const bool dmaChannelHalted =
      XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;

  const bool dmaToDeviceBusy = XAxiDma_Busy(&xDma, XAXIDMA_DMA_TO_DEVICE);

  // If the engine is doing a transfer, cannot submit
  if (not dmaChannelHalted and dmaToDeviceBusy) {
    hwWriteLock.unlock();
    return false;
  }

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
  hwWriteLock.unlock();
  return true;
}

bool Dma::readSimple(void *buf, size_t len) {
  hwReadLock.lock();
  XAxiDma_BdRing *ring = XAxiDma_GetRxRing(&xDma);

  if (not ring->HasDRE) {
    const uint32_t mask = xDma.MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN
                                            : ring->DataWidth - 1;

    if (reinterpret_cast<uintptr_t>(buf) & mask) {
      hwReadLock.unlock();
      return false;
    }
  }

  const bool dmaChannelHalted =
      XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK;
  const bool deviceToDmaBusy = XAxiDma_Busy(&xDma, XAXIDMA_DEVICE_TO_DMA);

  // If the engine is doing a transfer, cannot submit
  if (not dmaChannelHalted and deviceToDmaBusy) {
    hwReadLock.unlock();
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

  hwReadLock.unlock();
  return true;
}

Dma::Completion Dma::writeCompleteSimple() {
  Completion c;
  while (!(XAxiDma_IntrGetIrq(&xDma, XAXIDMA_DMA_TO_DEVICE) &
           XAXIDMA_IRQ_IOC_MASK))
    c.interrupts = irqs[mm2sInterrupt].irqController->waitForInterrupt(
        irqs[mm2sInterrupt]);

  hwWriteLock.lock();
  XAxiDma_IntrAckIrq(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);

  const XAxiDma_BdRing *ring = XAxiDma_GetTxRing(&xDma);
  const size_t bytesWritten =
      XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);
  hwWriteLock.unlock();
  c.bytes = bytesWritten;
  return c;
}

Dma::Completion Dma::readCompleteSimple() {
  Completion c;
  while (!(XAxiDma_IntrGetIrq(&xDma, XAXIDMA_DEVICE_TO_DMA) &
           XAXIDMA_IRQ_IOC_MASK))
    c.interrupts = irqs[s2mmInterrupt].irqController->waitForInterrupt(
        irqs[s2mmInterrupt]);

  hwReadLock.lock();
  XAxiDma_IntrAckIrq(&xDma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

  const XAxiDma_BdRing *ring = XAxiDma_GetRxRing(&xDma);
  const size_t bytesRead =
      XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);
  hwReadLock.unlock();

  c.bytes = bytesRead;
  return c;
}

void Dma::makeAccesibleFromVA(std::shared_ptr<MemoryBlock> mem) {
  // Only symmetric mapping supported currently
  if (isMemoryBlockAccesible(*mem, s2mmInterface) and
      isMemoryBlockAccesible(*mem, mm2sInterface))
    return;

  // Try mapping via FPGA-card (VFIO)
  if (not card->mapMemoryBlock(mem))
    throw RuntimeError("Memory not accessible by DMA");

  // Sanity-check if mapping worked, this shouldn't be neccessary
  if (not isMemoryBlockAccesible(*mem, s2mmInterface) or
      not isMemoryBlockAccesible(*mem, mm2sInterface))
    throw RuntimeError(
        "Mapping memory via card didn't work, but reported success?!");
}

bool Dma::isMemoryBlockAccesible(const MemoryBlock &mem,
                                 const std::string &interface) {
  auto &mm = MemoryManager::get();

  try {
    mm.findPath(getMasterAddrSpaceByInterface(interface), mem.getAddrSpaceId());
  } catch (const std::out_of_range &) {
    return false; // Not (yet) accessible
  }

  return true;
}

void Dma::dump() {
  Core::dump();

  logger->info(
      "S2MM_DMACR:  {:x}",
      XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_CR_OFFSET));
  logger->info(
      "S2MM_DMASR:  {:x}",
      XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_SR_OFFSET));

  if (!hasScatterGather())
    logger->info("S2MM_LENGTH: {:x}",
                 XAxiDma_ReadReg(xDma.RegBase,
                                 XAXIDMA_RX_OFFSET + XAXIDMA_BUFFLEN_OFFSET));

  logger->info(
      "MM2S_DMACR:  {:x}",
      XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_TX_OFFSET + XAXIDMA_CR_OFFSET));
  logger->info(
      "MM2S_DMASR:  {:x}",
      XAxiDma_ReadReg(xDma.RegBase, XAXIDMA_TX_OFFSET + XAXIDMA_SR_OFFSET));
}

void DmaFactory::parse(Core &ip, json_t *cfg) {
  NodeFactory::parse(ip, cfg);

  auto &dma = dynamic_cast<Dma &>(ip);

  dma.polling = dma.card->polling;
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
  int ret = json_unpack_ex(
      cfg, &err, 0,
      "{ s: { s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, s?: i, "
      "s?: i, s?: i, s?: i, s?: i } }",
      "parameters", "c_sg_include_stscntrl_strm", &dma.xConfig.HasStsCntrlStrm,
      "c_include_mm2s", &dma.xConfig.HasMm2S, "c_include_mm2s_dre",
      &dma.xConfig.HasMm2SDRE, "c_m_axi_mm2s_data_width",
      &dma.xConfig.Mm2SDataWidth, "c_include_s2mm", &dma.xConfig.HasS2Mm,
      "c_include_s2mm_dre", &dma.xConfig.HasS2MmDRE, "c_m_axi_s2mm_data_width",
      &dma.xConfig.S2MmDataWidth, "c_include_sg", &dma.xConfig.HasSg,
      "c_num_mm2s_channels", &dma.xConfig.Mm2sNumChannels,
      "c_num_s2mm_channels", &dma.xConfig.S2MmNumChannels, "c_micro_dma",
      &dma.xConfig.MicroDmaMode, "c_addr_width", &dma.xConfig.AddrWidth,
      "c_sg_length_width", &dma.xConfig.SgLengthWidth);
  if (ret != 0)
    throw ConfigError(cfg, err, "", "Failed to parse DMA configuration");

  dma.configDone = true;
}

static DmaFactory f;
