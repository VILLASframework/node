/** Driver for AXI Stream wrapper around RTDS_InterfaceModule (rtds_axis )
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <stdint.h>

#include "log.h"
#include "utils.h"

#include "fpga/rtds_axis.h"

void rtds_axis_dump(char *baseaddr)
{
	/* Check RTDS_Axis registers */
	uint32_t *rtds = (uint32_t *) baseaddr;

	uint32_t sr = rtds[RTDS_AXIS_SR_OFFSET/4];
	info("RTDS AXI Stream interface details");
	{ INDENT
		info("RTDS status:  %#08x", sr);
		{ INDENT
			info("Card detected:  %s", sr & RTDS_AXIS_SR_CARDDETECTED ? GRN("yes") : RED("no"));
			info("Link up:        %s", sr & RTDS_AXIS_SR_LINKUP ? GRN("yes") : RED("no"));
			info("TX queue full:  %s", sr & RTDS_AXIS_SR_TX_FULL ? RED("yes") : GRN("no"));
			info("TX in progress: %s", sr & RTDS_AXIS_SR_TX_INPROGRESS ? YEL("yes") : "no");
			info("Case running:   %s", sr & RTDS_AXIS_SR_CASE_RUNNING ? GRN("yes") : RED("no"));
		}

		info("RTDS control: %#08x", rtds[RTDS_AXIS_CR_OFFSET/4]);
		info("RTDS IRQ coalesc: %u", rtds[RTDS_AXIS_COALESC_OFFSET/4]);
		info("RTDS IRQ version: %#06x", rtds[RTDS_AXIS_VERSION_OFFSET/4]);
		info("RTDS IRQ multi-rate RTDS2AXIS: %u", rtds[RTDS_AXIS_MRATE_RTDS2AXIS/4]);
		info("RTDS IRQ multi-rate AXIS2RTDS: %u", rtds[RTDS_AXIS_MRATE_AXIS2RTDS/4]);

		info("RTDS timestep counter: %lu", (uint64_t) rtds[RTDS_AXIS_TSCNT_LOW_OFFSET/4] | (uint64_t) rtds[RTDS_AXIS_TSCNT_HIGH_OFFSET/4] << 32);
		info("RTDS timestep period:  %.3f uS", rtds_axis_dt(baseaddr) * 1e6);
	}
}

double rtds_axis_dt(char *baseaddr)
{
	uint32_t *rtds = (uint32_t *) baseaddr;

	return (double) rtds[RTDS_AXIS_TS_PERIOD_OFFSET/4] / RTDS_HZ;
}