/** AXI-PCIe Interrupt controller
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _INTC_H_
#define _INTC_H_

#include <xilinx/xintc.h>

int intc_init(struct ip *c);

int intc_enable(struct ip *c, uint32_t mask, int poll);

int intc_disable(struct ip *c, uint32_t mask);

uint64_t intc_wait(struct ip *c, int irq, int poll);

#endif /* _INTC_H_ */