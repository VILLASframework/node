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

int switch_init(XAxis_Switch *sw, char *baseaddr, int micnt, int sicnt)
{
	int ret;

	/* Setup AXI-stream switch */
	XAxis_Switch_Config sw_cfg = {
		.BaseAddress = (uintptr_t) baseaddr,
		.MaxNumMI = micnt,
		.MaxNumSI = sicnt
	};
	
	ret = XAxisScr_CfgInitialize(sw, &sw_cfg, (uintptr_t) baseaddr);
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

int switch_connect(XAxis_Switch *sw, int mi, int si)
{
	XAxisScr_RegUpdateDisable(sw);
	XAxisScr_MiPortEnable(sw, mi, si);
	XAxisScr_RegUpdateEnable(sw);

	return 0;
}
