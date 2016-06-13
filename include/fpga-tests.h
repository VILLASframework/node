/** Test procedures for VILLASfpga
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#ifndef _TESTS_H_
#define _TESTS_H_

#include "nodes/vfpga.h"

int test_intc(struct vfpga *f);

int test_xsg(struct vfpga *f, const char *name);

int test_fifo(struct vfpga *f);

int test_dma(struct vfpga *f);

int test_timer(struct vfpga *f);

int test_rtds_rtt(struct vfpga *f);

int test_rtds_cbuilder(struct vfpga *f);

#endif /* _TESTS_H_ */