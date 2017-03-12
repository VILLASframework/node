/** AXI-PCIe Interrupt controller
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

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

int intc_init(struct fpga_ip *c);

int intc_destroy(struct fpga_ip *c);

int intc_enable(struct fpga_ip *c, uint32_t mask, int poll);

int intc_disable(struct fpga_ip *c, uint32_t mask);

uint64_t intc_wait(struct fpga_ip *c, int irq);

/** @} */