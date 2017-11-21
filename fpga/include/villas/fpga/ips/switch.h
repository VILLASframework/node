/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @file
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <jansson.h>
#include <xilinx/xaxis_switch.h>

#include "list.h"

/* Forward declarations */
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

int switch_start(struct fpga_ip *c);

/** Initialize paths which have been parsed by switch_parse() */
int switch_init_paths(struct fpga_ip *c);

int switch_destroy(struct fpga_ip *c);

int switch_parse(struct fpga_ip *c, json_t *cfg);

int switch_connect(struct fpga_ip *c, struct fpga_ip *mi, struct fpga_ip *si);

int switch_disconnect(struct fpga_ip *c, struct fpga_ip *mi, struct fpga_ip *si);

/** @} */
