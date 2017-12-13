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

#include "fpga/ip.h"
#include "fpga/card.h"
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

bool InterruptController::start()
{
	const uintptr_t base = getBaseaddr();

	num_irqs = vfio_pci_msi_init(&card->vfio_device, efds);
	if (num_irqs < 0)
		return false;

	if(vfio_pci_msi_find(&card->vfio_device, nos) != 0)
		return false;

	/* For each IRQ */
	for (int i = 0; i < num_irqs; i++) {
		/* Pin to core */
		if(kernel_irq_setaffinity(nos[i], card->affinity, NULL) !=0)
			serror("Failed to change affinity of VFIO-MSI interrupt");

		/* Setup vector */
		XIntc_Out32(base + XIN_IVAR_OFFSET + i * 4, i);
	}

	XIntc_Out32(base + XIN_IMR_OFFSET, 0);		/* Use manual acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IAR_OFFSET, 0xFFFFFFFF); /* Acknowlege all pending IRQs manually */
	XIntc_Out32(base + XIN_IMR_OFFSET, 0xFFFFFFFF); /* Use fast acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IER_OFFSET, 0x00000000); /* Disable all IRQs by default */
	XIntc_Out32(base + XIN_MER_OFFSET, XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

	debug(4, "FPGA: enabled interrupts");

	return true;
}

int
InterruptController::enableInterrupt(InterruptController::IrqMaskType mask, bool polling)
{
	uint32_t ier, imr;
	const uintptr_t base = getBaseaddr();

	/* Current state of INTC */
	ier = XIntc_In32(base + XIN_IER_OFFSET);
	imr = XIntc_In32(base + XIN_IMR_OFFSET);

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

	debug(3, "New ier = %#x", XIntc_In32(base + XIN_IER_OFFSET));
	debug(3, "New imr = %#x", XIntc_In32(base + XIN_IMR_OFFSET));
	debug(3, "New isr = %#x", XIntc_In32(base + XIN_ISR_OFFSET));

	debug(8, "FPGA: Interupts enabled: mask=%#x polling=%d", mask, polling);

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

uint64_t InterruptController::waitForInterrupt(int irq)
{
	assert(irq < maxIrqs);

	const uintptr_t base = getBaseaddr();

	if (this->polling[irq]) {
		uint32_t isr, mask = 1 << irq;

		do {
			isr = XIntc_In32(base + XIN_ISR_OFFSET);
			pthread_testcancel();
		} while ((isr & mask) != mask);

		XIntc_Out32(base + XIN_IAR_OFFSET, mask);

		return 1;
	}
	else {
		uint64_t cnt;
		ssize_t ret = read(efds[irq], &cnt, sizeof(cnt));
		if (ret != sizeof(cnt))
			return 0;

		return cnt;
	}
}

} // namespace ip
} // namespace fpga
} // namespace villas
