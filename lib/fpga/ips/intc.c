/** AXI-PCIe Interrupt controller
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <unistd.h>

#include "config.h" 
#include "log.h"
#include "plugin.h"

#include "nodes/fpga.h"

#include "kernel/vfio.h" 
#include "kernel/kernel.h" 

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/intc.h"

int intc_init(struct fpga_ip *c)
{
	int ret;

	struct fpga_card *f = c->card;
	struct intc *intc = &c->intc;

	uintptr_t base = (uintptr_t) f->map + c->baseaddr;

	if (c != f->intc)
		error("There can be only one interrupt controller per FPGA");

	intc->num_irqs = vfio_pci_msi_init(&f->vd, intc->efds);
	if (intc->num_irqs < 0)
		return -1;

	ret = vfio_pci_msi_find(&f->vd, intc->nos);
	if (ret)
		return -2;

	/* For each IRQ */
	for (int i = 0; i < intc->num_irqs; i++) {
		/* Pin to core */
		ret = kernel_irq_setaffinity(intc->nos[i], f->affinity, NULL);
		if (ret)
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
	
	return 0;
}

int intc_destroy(struct fpga_ip *c)
{
	struct fpga_card *f = c->card;
	struct intc *intc = &c->intc;

	vfio_pci_msi_deinit(&f->vd, intc->efds);
	
	return 0;
}

int intc_enable(struct fpga_ip *c, uint32_t mask, int flags)
{
	struct fpga_card *f = c->card;
	struct intc *intc = &c->intc;

	uint32_t ier, imr;
	uintptr_t base = (uintptr_t) f->map + c->baseaddr;

	/* Current state of INTC */
	ier = XIntc_In32(base + XIN_IER_OFFSET);
	imr = XIntc_In32(base + XIN_IMR_OFFSET);

	/* Clear pending IRQs */
	XIntc_Out32(base + XIN_IAR_OFFSET, mask);
	
	for (int i = 0; i < intc->num_irqs; i++) {
		if (mask & (1 << i))
			intc->flags[i] = flags;
	}

	if (flags & INTC_POLLING) {
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

	debug(8, "FPGA: Interupt enabled: mask=%#x flags=%#x", mask, flags);

	return 0;
}

int intc_disable(struct fpga_ip *c, uint32_t mask)
{
	struct fpga_card *f = c->card;

	uintptr_t base = (uintptr_t) f->map + c->baseaddr;
	uint32_t ier = XIntc_In32(base + XIN_IER_OFFSET);

	XIntc_Out32(base + XIN_IER_OFFSET, ier & ~mask);

	return 0;
}

uint64_t intc_wait(struct fpga_ip *c, int irq)
{
	struct fpga_card *f = c->card;
	struct intc *intc = &c->intc;

	uintptr_t base = (uintptr_t) f->map + c->baseaddr;

	if (intc->flags[irq] & INTC_POLLING) {
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
		ssize_t ret = read(intc->efds[irq], &cnt, sizeof(cnt));
		if (ret != sizeof(cnt))
			return 0;
		
		return cnt;
	}
}

static struct plugin p = {
	.name		= "Xilinx's programmable interrupt controller",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv	= { "acs.eonerc.rwth-aachen.de", "user", "axi_pcie_intc", NULL },
		.type	= FPGA_IP_TYPE_MISC,
		.init	= intc_init,
		.destroy = intc_destroy
	}
};

REGISTER_PLUGIN(&p)