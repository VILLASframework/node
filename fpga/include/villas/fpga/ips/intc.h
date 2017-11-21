/** AXI-PCIe Interrupt controller
 *
 * @file
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
