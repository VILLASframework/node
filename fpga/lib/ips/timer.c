/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
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

#include "config.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/timer.h"

int timer_start(struct fpga_ip *c)
{
	struct fpga_card *f = c->card;
	struct timer *tmr = (struct timer *) c->_vd;

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
