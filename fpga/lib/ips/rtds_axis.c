/** Driver for AXI Stream wrapper around RTDS_InterfaceModule (rtds_axis )
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

#include <stdint.h>

#include "log.h"
#include "utils.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/rtds_axis.h"

void rtds_axis_dump(struct fpga_ip *c)
{
	/* Check RTDS_Axis registers */
	uint32_t *regs = (uint32_t *) (c->card->map + c->baseaddr);

	uint32_t sr = regs[RTDS_AXIS_SR_OFFSET/4];
	info("RTDS AXI Stream interface details");
	{ INDENT
		info("RTDS status:  %#08x", sr);
		{ INDENT
			info("Card detected:  %s", sr & RTDS_AXIS_SR_CARDDETECTED ?  CLR_GRN("yes") : CLR_RED("no"));
			info("Link up:        %s", sr & RTDS_AXIS_SR_LINKUP ?        CLR_GRN("yes") : CLR_RED("no"));
			info("TX queue full:  %s", sr & RTDS_AXIS_SR_TX_FULL ?       CLR_RED("yes") : CLR_GRN("no"));
			info("TX in progress: %s", sr & RTDS_AXIS_SR_TX_INPROGRESS ? CLR_YEL("yes") :         "no");
			info("Case running:   %s", sr & RTDS_AXIS_SR_CASE_RUNNING ?  CLR_GRN("yes") : CLR_RED("no"));
		}

		info("RTDS control: %#08x", regs[RTDS_AXIS_CR_OFFSET/4]);
		info("RTDS IRQ coalesc: %u", regs[RTDS_AXIS_COALESC_OFFSET/4]);
		info("RTDS IRQ version: %#06x", regs[RTDS_AXIS_VERSION_OFFSET/4]);
		info("RTDS IRQ multi-rate: %u", regs[RTDS_AXIS_MRATE/4]);

		info("RTDS timestep counter: %lu", (uint64_t) regs[RTDS_AXIS_TSCNT_LOW_OFFSET/4] | (uint64_t) regs[RTDS_AXIS_TSCNT_HIGH_OFFSET/4] << 32);
		info("RTDS timestep period:  %.3f uS", rtds_axis_dt(c) * 1e6);
	}
}

double rtds_axis_dt(struct fpga_ip *c)
{
	uint32_t *regs = (uint32_t *) (c->card->map + c->baseaddr);
	uint16_t dt = regs[RTDS_AXIS_TS_PERIOD_OFFSET/4];

	return (dt == 0xFFFF) ? -1.0 : (double) dt / RTDS_HZ;
}

static struct plugin p = {
	.name		= "RTDS's AXI4-Stream - GTFPGA interface",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv	= { "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL },
		.type	= FPGA_IP_TYPE_INTERFACE,
		.dump	= rtds_axis_dump,
		.size	= 0
	}
};

REGISTER_PLUGIN(&p)
