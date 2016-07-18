/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include "config.h"

#include "fpga/ip.h"
#include "fpga/timer.h"

int timer_init(struct ip *c)
{
	struct timer *tmr = &c->timer;
	struct fpga *f = c->card;

	XTmrCtr *xtmr = &tmr->inst;
	XTmrCtr_Config xtmr_cfg = {
		.BaseAddress = (uintptr_t) f->map + c->baseaddr,
		.SysClockFreqHz = FPGA_AXI_HZ
	};

	XTmrCtr_CfgInitialize(xtmr, &xtmr_cfg, (uintptr_t) f->map + c->baseaddr);
	XTmrCtr_InitHw(xtmr);
	
	return 0;
}

static struct ip_type ip = {
	.vlnv = { "xilinx.com", "ip", "axi_timer", NULL },
	.init = timer_init
};

REGISTER_IP_TYPE(&ip)