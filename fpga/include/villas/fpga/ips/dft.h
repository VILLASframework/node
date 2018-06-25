/** Moving window / Recursive DFT implementation based on HLS
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Steffen Vogel
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

#include <xilinx/xhls_dft.h>

/* Forward declaration */
struct ip;

struct dft {
	XHls_dft inst;

	int period; /* in samples */
	int num_harmonics;
	float *fharmonics;
	int decimation;
};

int dft_parse(struct fpga_ip *c, json_t *cfg);

int dft_start(struct fpga_ip *c);

int dft_stop(struct fpga_ip *c);

int dft_destroy(struct fpga_ip *c);

/** @} */
