/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

#include "config.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/timer.h"

int timer_start(struct fpga_ip *c)
{
	struct fpga_card *f = c->card;
	struct timer *tmr = (struct timer *) &c->_vd;

	XTmrCtr *xtmr = &tmr->inst;
	XTmrCtr_Config xtmr_cfg = {
		.BaseAddress = (uintptr_t) f->map + c->baseaddr,
		.SysClockFreqHz = FPGA_AXI_HZ
	};

	XTmrCtr_CfgInitialize(xtmr, &xtmr_cfg, (uintptr_t) f->map + c->baseaddr);
	XTmrCtr_InitHw(xtmr);
	
	return 0;
}

static struct plugin p = {
	.name		= "Xilinx's programmable timer / counter",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv	= { "xilinx.com", "ip", "axi_timer", NULL },
		.type	= FPGA_IP_TYPE_MISC,
		.start	= timer_start,
		.size	= sizeof(struct timer)
	}
};

REGISTER_PLUGIN(&p)