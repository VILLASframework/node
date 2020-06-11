/** AXI-PCIe Interrupt controller
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <unistd.h>
#include <errno.h>

#include <villas/config.h>
#include <villas/plugin.hpp>

#include <villas/kernel/kernel.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/intc.hpp>

namespace villas {
namespace fpga {
namespace ip {


// instantiate factory to make available to plugin infrastructure
static InterruptControllerFactory factory;

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

	if(not card->vfioDevice->pciMsiFind(nos)) {
		return false;
	}

	/* For each IRQ */
	for (int i = 0; i < num_irqs; i++) {

		/* Try pinning to core */
		int ret = kernel::irq_setaffinity(nos[i], card->affinity, nullptr);

		switch(ret) {
		case 0:
			// everything is fine
			break;
		case EACCES:
			logger->warn("No permission to change affinity of VFIO-MSI interrupt, "
			             "performance may be degraded!");
			break;
		default:
			logger->error("Failed to change affinity of VFIO-MSI interrupt");
			return false;
		}

		/* Setup vector */
		XIntc_Out32(base + XIN_IVAR_OFFSET + i * 4, i);
	}

	XIntc_Out32(base + XIN_IMR_OFFSET, 0x00000000); /* Use manual acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IAR_OFFSET, 0xFFFFFFFF); /* Acknowlege all pending IRQs manually */
	XIntc_Out32(base + XIN_IMR_OFFSET, 0xFFFFFFFF); /* Use fast acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IER_OFFSET, 0x00000000); /* Disable all IRQs by default */
	XIntc_Out32(base + XIN_MER_OFFSET, XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

	logger->debug("enabled interrupts");


	return true;
}

bool
InterruptController::enableInterrupt(InterruptController::IrqMaskType mask, bool polling)
{
	const uintptr_t base = getBaseAddr(registerMemory);

	/* Current state of INTC */
	const uint32_t ier = XIntc_In32(base + XIN_IER_OFFSET);
	const uint32_t imr = XIntc_In32(base + XIN_IMR_OFFSET);

	/* Clear pending IRQs */
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

int
InterruptController::waitForInterrupt(int irq)
{
	assert(irq < maxIrqs);

	const uintptr_t base = getBaseAddr(registerMemory);

	if (this->polling[irq]) {
		uint32_t isr, mask = 1 << irq;

		do {
			// poll status register
			isr = XIntc_In32(base + XIN_ISR_OFFSET);
			pthread_testcancel();
		} while ((isr & mask) != mask);

		// acknowledge interrupt
		XIntc_Out32(base + XIN_IAR_OFFSET, mask);

		// we can only tell that there has been (at least) one interrupt
		return 1;
	}
	else {
		uint64_t count;

		// block until there has been an interrupt, read number of interrupts
		ssize_t ret = read(efds[irq], &count, sizeof(count));
		if (ret != sizeof(count))
			return -1;

		return static_cast<int>(count);
	}
}

} // namespace ip
} // namespace fpga
} // namespace villas
