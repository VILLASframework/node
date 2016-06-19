/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include "fpga/switch.h"
#include "fpga/ip.h"

int switch_init(struct ip *c)
{
	XAxis_Switch *sw = &c->sw;
	int ret;

	/* Setup AXI-stream switch */
	XAxis_Switch_Config sw_cfg = {
		.BaseAddress = (uintptr_t) c->card->map + c->baseaddr,
		.MaxNumMI = sw->Config.MaxNumMI,
		.MaxNumSI = sw->Config.MaxNumSI
	};
	
	ret = XAxisScr_CfgInitialize(sw, &sw_cfg, (uintptr_t) c->card->map + c->baseaddr);
	if (ret != XST_SUCCESS)
		return -1;

#if 0
	ret = XAxisScr_SelfTest(sw);
	if (ret != TRUE)
		return -1;
#endif

	/* Disable all masters */
	XAxisScr_RegUpdateDisable(sw);
	XAxisScr_MiPortDisableAll(sw);
	XAxisScr_RegUpdateEnable(sw);

	return 0;
}

int switch_connect(struct ip *c, struct ip *mi, struct ip *si)
{
	XAxis_Switch *sw = &c->sw;

	XAxisScr_RegUpdateDisable(sw);
	XAxisScr_MiPortEnable(sw, mi->port, si->port);
	XAxisScr_RegUpdateEnable(sw);

	return 0;
}
