/** AXI-PCIe Interrupt controller
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
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

#include "config.h"
#include "log.h"
#include "plugin.hpp"

#include "kernel/vfio.h"
#include "kernel/kernel.h"

#include "fpga/card.hpp"
#include "fpga/ips/intc.hpp"

namespace villas {
namespace fpga {
namespace ip {


// instantiate factory to make available to plugin infrastructure
static InterruptControllerFactory factory;

InterruptController::~InterruptController()
{
	vfio_pci_msi_deinit(&card->vfio_device , this->efds);
}

bool InterruptController::init()
{
	const uintptr_t base = getBaseaddr();
	auto logger = getLogger();

	num_irqs = vfio_pci_msi_init(&card->vfio_device, efds);
	if (num_irqs < 0)
		return false;

	if(vfio_pci_msi_find(&card->vfio_device, nos) != 0)
		return false;

	/* For each IRQ */
	for (int i = 0; i < num_irqs; i++) {
		/* Pin to core */
		if(kernel_irq_setaffinity(nos[i], card->affinity, nullptr) != 0) {
			logger->error("Failed to change affinity of VFIO-MSI interrupt");
			return false;
		}

		/* Setup vector */
		XIntc_Out32(base + XIN_IVAR_OFFSET + i * 4, i);
	}

	XIntc_Out32(base + XIN_IMR_OFFSET, 0);		/* Use manual acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IAR_OFFSET, 0xFFFFFFFF); /* Acknowlege all pending IRQs manually */
	XIntc_Out32(base + XIN_IMR_OFFSET, 0xFFFFFFFF); /* Use fast acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IER_OFFSET, 0x00000000); /* Disable all IRQs by default */
	XIntc_Out32(base + XIN_MER_OFFSET, XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

	logger->debug("enabled interrupts");;

	return true;
}

int
InterruptController::enableInterrupt(InterruptController::IrqMaskType mask, bool polling)
{
	auto logger = getLogger();
	const uintptr_t base = getBaseaddr();

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

int
InterruptController::disableInterrupt(InterruptController::IrqMaskType mask)
{
	const uintptr_t base = getBaseaddr();
	uint32_t ier = XIntc_In32(base + XIN_IER_OFFSET);

	XIntc_Out32(base + XIN_IER_OFFSET, ier & ~mask);

	return true;
}

int InterruptController::waitForInterrupt(int irq)
{
	assert(irq < maxIrqs);

	const uintptr_t base = getBaseaddr();

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
