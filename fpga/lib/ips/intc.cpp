/* AXI-PCIe Interrupt controller
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <villas/config.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/intc.hpp>
#include <villas/fpga/pcie_card.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/plugin.hpp>

using namespace villas::fpga::ip;

InterruptController::~InterruptController() {}

bool InterruptController::stop() {
  return this->vfioDevice->pciMsiDeinit(irq_vectors[0].eventFds) > 0;
}

bool InterruptController::init() {
  PCIeCard *pciecard = dynamic_cast<PCIeCard *>(card);
  this->vfioDevice = pciecard->vfioDevice;

  const uintptr_t base = getBaseAddr(registerMemory);
  kernel::vfio::Device::IrqVectorInfo irq_vector = {0};
  irq_vector.numFds = this->vfioDevice->pciMsiInit(irq_vector.eventFds);
  irq_vector.automask = true;
  irq_vectors.push_back(irq_vector);

  if (not this->vfioDevice->pciMsiFind(nos)) {
    return false;
  }

  if (irq_vector.numFds < 0)
    return false;

  // For each IRQ
  for (size_t i = 0; i < irq_vector.numFds; i++) {

    // Try pinning to core
    int ret = kernel::setIRQAffinity(nos[i], pciecard->affinity, nullptr);

    switch (ret) {
    case 0:
      // Everything is fine
      break;
    case EACCES:
      logger->warn("No permission to change affinity of VFIO-MSI interrupt, "
                   "performance may be degraded!");
      break;
    default:
      logger->error("Failed to change affinity of VFIO-MSI interrupt");
      return false;
    }

    // Setup vector
    XIntc_Out32(base + XIN_IVAR_OFFSET + i * 4, i);
  }

  XIntc_Out32(base + XIN_IMR_OFFSET,
              0x00000000); // Use manual acknowlegement for all IRQs
  XIntc_Out32(base + XIN_IAR_OFFSET,
              0xFFFFFFFF); // Acknowlege all pending IRQs manually
  XIntc_Out32(base + XIN_IMR_OFFSET,
              0xFFFFFFFF); // Use fast acknowlegement for all IRQs
  XIntc_Out32(base + XIN_IER_OFFSET, 0x00000000); // Disable all IRQs by default
  XIntc_Out32(base + XIN_MER_OFFSET,
              XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

  logger->debug("enabled interrupts");

  return true;
}

bool InterruptController::enableInterrupt(InterruptController::IrqMaskType mask,
                                          bool polling) {
  const uintptr_t base = getBaseAddr(registerMemory);

  // Current state of INTC
  const uint32_t ier = XIntc_In32(base + XIN_IER_OFFSET);
  const uint32_t imr = XIntc_In32(base + XIN_IMR_OFFSET);

  // Clear pending IRQs
  XIntc_Out32(base + XIN_IAR_OFFSET, mask);

  for (size_t i = 0; i < irq_vectors[0].numFds; i++) {
    if (mask & (1 << i))
      this->polling[i] = polling;
  }

  if (polling) {
    XIntc_Out32(base + XIN_IMR_OFFSET, imr & ~mask);
    XIntc_Out32(base + XIN_IER_OFFSET, ier & ~mask);
  } else {
    XIntc_Out32(base + XIN_IER_OFFSET, ier | mask);
    XIntc_Out32(base + XIN_IMR_OFFSET, imr | mask);
  }

  logger->debug("New ier = {:x}", XIntc_In32(base + XIN_IER_OFFSET));
  logger->debug("New imr = {:x}", XIntc_In32(base + XIN_IMR_OFFSET));
  logger->debug("New isr = {:x}", XIntc_In32(base + XIN_ISR_OFFSET));
  logger->debug("Interupts enabled: mask={:x} polling={:d}", mask, polling);

  return true;
}

bool InterruptController::disableInterrupt(
    InterruptController::IrqMaskType mask) {
  const uintptr_t base = getBaseAddr(registerMemory);
  uint32_t ier = XIntc_In32(base + XIN_IER_OFFSET);

  XIntc_Out32(base + XIN_IER_OFFSET, ier & ~mask);

  return true;
}

// Assuming only one interrupt vector
ssize_t InterruptController::waitForInterrupt(int irq) {
  assert(irq < maxIrqs);

  if (this->polling[irq]) {
    const uintptr_t base = getBaseAddr(registerMemory);
    uint32_t isr, mask = 1 << irq;

    do {
      // Poll status register
      isr = XIntc_In32(base + XIN_ISR_OFFSET);
      pthread_testcancel();
    } while ((isr & mask) != mask);

    // Acknowledge interrupt
    XIntc_Out32(base + XIN_IAR_OFFSET, mask);

    // We can only tell that there has been (at least) one interrupt
    return 1;
  } else {
    uint64_t count;
    int sret;
    fd_set rfds;
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    FD_ZERO(&rfds);
    FD_SET(irq_vectors[0].eventFds[irq], &rfds);
    logger->debug("Waiting for interrupt fd {}", irq_vectors[0].eventFds[irq]);

    sret = select(irq_vectors[0].eventFds[irq] + 1, &rfds, NULL, NULL, &tv);
    if (sret == -1) {
      logger->error("select() failed: {}", strerror(errno));
      return -1;
    } else if (sret == 0) {
      logger->warn("timeout waiting for interrupt {}", irq);
      return -1;
    }
    // Block until there has been an interrupt, read number of interrupts
    ssize_t ret = read(irq_vectors[0].eventFds[irq], &count, sizeof(count));
    if (ret != sizeof(count)) {
      logger->error("Read failure on interrupt {}, {}", irq, ret);
      return -1;
    } else {
      logger->debug("Automask: {}", irq_vectors[0].automask);
      if (irq_vectors[0].automask) {
        struct vfio_irq_set irqSet = {0};
        irqSet.argsz = sizeof(struct vfio_irq_set);
        irqSet.flags = (VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_UNMASK);
        irqSet.index = 0;
        irqSet.start = irq;
        irqSet.count = 1;
        int ret = ioctl(this->vfioDevice->getFileDescriptor(),
                        VFIO_DEVICE_SET_IRQS, &irqSet);
        if (ret < 0) {
          logger->error("Failed to unmask IRQ {}", 0);
        }
      }
    }

    return count;
  }
}

static char n[] = "intc";
static char d[] = "Xilinx's programmable interrupt controller";
static char v[] = "xilinx.com:module_ref:axi_pcie_intc:";
static CorePlugin<InterruptController, n, d, v> f;
