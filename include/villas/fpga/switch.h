/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _SWITCH_H_
#define _SWITCH_H_

#include <xilinx/xaxis_switch.h>

int switch_init(XAxis_Switch *sw, char *baseaddr, int micnt, int sicnt);

int switch_connect(XAxis_Switch *sw, int mi, int si);

#endif /* _SWITCH_H_ */