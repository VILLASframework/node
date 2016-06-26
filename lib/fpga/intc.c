/** AXI-PCIe Interrupt controller
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <unistd.h>
 
#include "log.h"
#include "nodes/fpga.h"
#include "kernel/vfio.h" 
#include "fpga/ip.h"
#include "fpga/intc.h"

int intc_init(struct ip *c)
{
	struct fpga *f = c->card;
	int ret;

	if (c != f->intc)
		error("There can be only one interrupt controller per FPGA");

	uintptr_t base = (uintptr_t) f->map + c->baseaddr;

	/* Setup IRQs */
	for (int i = 0; i < f->vd.irqs[VFIO_PCI_MSI_IRQ_INDEX].count; i++) {
		/* Setup vector */
		XIntc_Out32(base + XIN_IVAR_OFFSET + i * 4, i);
		
		/* Register eventfd with VFIO */
		ret = vfio_pci_msi_fd(&f->vd, (1 << i));
		if (ret < 0)
			serror("Failed to create eventfd for IRQ: ret=%d", f->vd.msi_efds[i]);
	}

	XIntc_Out32(base + XIN_IMR_OFFSET, 0);		/* Use manual acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IAR_OFFSET, 0xFFFFFFFF); /* Acknowlege all pending IRQs manually */
	XIntc_Out32(base + XIN_IMR_OFFSET, 0xFFFFFFFF); /* Use fast acknowlegement for all IRQs */
	XIntc_Out32(base + XIN_IER_OFFSET, 0x00000000); /* Disable all IRQs by default */
	XIntc_Out32(base + XIN_MER_OFFSET, XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

	debug(4, "FPGA: enabled interrupts");
	
	return 0;
}

int intc_enable(struct ip *c, uint32_t mask, int poll)
{
	struct fpga *f = c->card;

	uint32_t ier, imr;
	uintptr_t base = (uintptr_t) f->map + c->baseaddr;

	/* Current state of INTC */
	ier = XIntc_In32(base + XIN_IER_OFFSET);
	imr = XIntc_In32(base + XIN_IMR_OFFSET);

	/* Clear pending IRQs */
	XIntc_Out32(base + XIN_IAR_OFFSET, mask);
	
	if (poll)
		XIntc_Out32(base + XIN_IMR_OFFSET, imr & ~mask);
	else
		XIntc_Out32(base + XIN_IER_OFFSET, ier | mask);

	debug(8, "FPGA: Interupt enabled: %#x", mask);

	return 0;
}

int intc_disable(struct ip *c, uint32_t mask)
{
	struct fpga *f = c->card;

	uintptr_t base = (uintptr_t) f->map + c->baseaddr;

	XIntc_Out32(base + XIN_IER_OFFSET, XIntc_In32(base + XIN_IER_OFFSET) & ~mask);

	return 0;
}

uint64_t intc_wait(struct ip *c, int irq, int poll)
{
	struct fpga *f = c->card;

	uintptr_t base = (uintptr_t) f->map + c->baseaddr;
	uint64_t cnt;
	
	if (poll) {
		uint32_t mask = 1 << irq;
		uint32_t isr;

		do { /* Wait for IRQ to occur */
			isr = XIntc_In32(base + XIN_ISR_OFFSET);
		} while (!(isr & mask));

		/* Acknowlege */
		XIntc_Out32(base + XIN_IAR_OFFSET, mask);
		
		cnt = 1;
	}
	else {
		ssize_t ret = read(f->vd.msi_efds[irq], &cnt, sizeof(cnt));
		if (ret != sizeof(cnt))
			return 0;
	}

	return cnt;
}

static struct ip_type ip = {
	.vlnv = { "acs.eonerc.rwth-aachen.de", "user", "axi_pcie_intc", NULL },
	.init = intc_init
};

REGISTER_IP_TYPE(&ip)
