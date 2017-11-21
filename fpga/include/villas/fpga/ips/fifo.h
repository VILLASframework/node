/** FIFO related helper functions
 *
 * These functions present a simpler interface to Xilinx' FIFO driver (XLlFifo_*)
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

#include <sys/types.h>

#include <xilinx/xstatus.h>
#include <xilinx/xllfifo.h>

struct fifo {
	XLlFifo inst;

	uint32_t baseaddr_axi4;
};

/* Forward declarations */
struct ip;

int fifo_start(struct fpga_ip *c);

ssize_t fifo_write(struct fpga_ip *c, char *buf, size_t len);

ssize_t fifo_read(struct fpga_ip *c, char *buf, size_t len);

/** @} */
