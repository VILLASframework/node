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

#include "list.h"

/* Forward declaration */
struct ip;

struct sw_path {
	const char *in;
	const char *out;
};

struct sw {
	XAxis_Switch inst;

	int num_ports;
	struct list paths;
};

struct ip;

int switch_init(struct ip *c);

/** Initialize paths which have been parsed by switch_parse() */
int switch_init_paths(struct ip *c);

void switch_destroy(struct ip *c);

int switch_parse(struct ip *c);

int switch_connect(struct ip *c, struct ip *mi, struct ip *si);

int switch_disconnect(struct ip *c, struct ip *mi, struct ip *si);

#endif /* _SWITCH_H_ */