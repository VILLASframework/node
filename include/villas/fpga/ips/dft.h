/** Moving window / Recursive DFT implementation based on HLS
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 */
/**
 * @addtogroup fpga VILLASfpga
 * @{
 **********************************************************************************/

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

int dft_parse(struct fpga_ip *c);

int dft_init(struct fpga_ip *c);

int dft_destroy(struct fpga_ip *c);

/** @} */