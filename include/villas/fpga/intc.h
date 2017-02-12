/** AXI-PCIe Interrupt controller
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _FPGA_INTC_H_
#define _FPGA_INTC_H_

#include <xilinx/xintc.h>

enum intc_flags {
	INTC_ENABLED = (1 << 0),
	INTC_POLLING = (1 << 1)
};

struct intc {
	int num_irqs;		/**< Number of available MSI vectors */

	int efds[32];		/**< Event FDs */
	int nos[32];		/**< Interrupt numbers from /proc/interrupts */
	
	int flags[32];		/**< Mask of intc_flags */
};

int intc_init(struct ip *c);

void intc_destroy(struct ip *c);

int intc_enable(struct ip *c, uint32_t mask, int poll);

int intc_disable(struct ip *c, uint32_t mask);

uint64_t intc_wait(struct ip *c, int irq);

#endif /* _FPGA_INTC_H_ */