/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <xilinx/xtmrctr.h>

/* Forward declarations */
struct fpga_ip;

struct timer {
	XTmrCtr inst;
};

int timer_start(struct fpga_ip *c);

/** @} */
