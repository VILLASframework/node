/* AXI-PCIe Interrupt controller
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>
#include <errno.h>

#include <villas/config.hpp>
#include <villas/plugin.hpp>

#include <villas/kernel/kernel.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/intc.hpp>
#include <villas/fpga/pcie_card.hpp>

using namespace villas::fpga::ip;

InterruptController::~InterruptController()
{
	card->vfioDevice->pciMsiDeinit(this->efds);
}

bool
InterruptController::init()
{
	const uintptr_t base = getBaseAddr(registerMemory);

	num_irqs = card->vfioDevice->pciMsiInit(efds);
	if (num_irqs < 0)
		return false;

	if (not card->vfioDevice->pciMsiFind(nos)) {
		return false;
	}

	// For each IRQ
	for (int i = 0; i < num_irqs; i++) {

		// Try pinning to core
		PCIeCard* pciecard = dynamic_cast<PCIeCard*>(card);
		int ret = kernel::setIRQAffinity(nos[i], pciecard->affinity, nullptr);

		switch(ret) {
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

	XIntc_Out32(base + XIN_IMR_OFFSET, 0x00000000); // Use manual acknowlegement for all IRQs
	XIntc_Out32(base + XIN_IAR_OFFSET, 0xFFFFFFFF); // Acknowlege all pending IRQs manually
	XIntc_Out32(base + XIN_IMR_OFFSET, 0xFFFFFFFF); // Use fast acknowlegement for all IRQs
	XIntc_Out32(base + XIN_IER_OFFSET, 0x00000000); // Disable all IRQs by default
	XIntc_Out32(base + XIN_MER_OFFSET, XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

	logger->debug("enabled interrupts");

	return true;
}

bool
InterruptController::enableInterrupt(InterruptController::IrqMaskType mask, bool polling)
{
	const uintptr_t base = getBaseAddr(registerMemory);

	// Current state of INTC
	const uint32_t ier = XIntc_In32(base + XIN_IER_OFFSET);
	const uint32_t imr = XIntc_In32(base + XIN_IMR_OFFSET);

	// Clear pending IRQs
	XIntc_Out32(base + XIN_IAR_OFFSET, mask);

	for (int i = 0; i < num_irqs; i++) {
		if (mask & (1 << i))
			this->polling[i] = polling;
	}

	if (polling) {
		XIntc_Out32(base + XIN_IMR_OFFSET, imr & ~mask);
		XIntc_Out32(base + XIN_IER_OFFSET, ier & ~mask);
	}
	else {
		XIntc_Out32(base + XIN_IER_OFFSET, ier | mask);
		XIntc_Out32(base + XIN_IMR_OFFSET, imr | mask);
	}

	logger->debug("New ier = {:x}", XIntc_In32(base + XIN_IER_OFFSET));
	logger->debug("New imr = {:x}", XIntc_In32(base + XIN_IMR_OFFSET));
	logger->debug("New isr = {:x}", XIntc_In32(base + XIN_ISR_OFFSET));
	logger->debug("Interupts enabled: mask={:x} polling={:d}", mask, polling);

	return true;
}

bool
InterruptController::disableInterrupt(InterruptController::IrqMaskType mask)
{
	const uintptr_t base = getBaseAddr(registerMemory);
	uint32_t ier = XIntc_In32(base + XIN_IER_OFFSET);

	XIntc_Out32(base + XIN_IER_OFFSET, ier & ~mask);

	return true;
}

ssize_t InterruptController::waitForInterrupt(int irq) {
  assert(irq < maxIrqs);

  const uintptr_t base = getBaseAddr(registerMemory);

  if (this->polling[irq]) {
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
    FD_SET(efds[irq], &rfds);

    sret = select(efds[irq] + 1, &rfds, NULL, NULL, &tv);
    if (sret == -1) {
      logger->error("select() failed: {}", strerror(errno));
      return -1;
    } else if (sret == 0) {
      logger->warn("timeout waiting for interrupt {}", irq);
      return -1;
    }
    // Block until there has been an interrupt, read number of interrupts
    ssize_t ret = read(efds[irq], &count, sizeof(count));
    if (ret != sizeof(count)) {
      logger->error("Failed to read from eventfd: {}", strerror(errno));
      return -1;
    }

    logger->debug("Received {} interrupts on {}", count, irq);

    return count;
  }
}

static char n[] = "intc";
static char d[] = "Xilinx's programmable interrupt controller";
static char v[] = "xilinx.com:module_ref:axi_pcie_intc:";
static CorePlugin<InterruptController, n, d, v> f;
