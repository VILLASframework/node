/** FIFO related helper functions
 *
 * These functions present a simpler interface to Xilinx' FIFO driver (XLlFifo_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _FIFO_H_
#define _FIFO_H_

#include <sys/types.h>

#include <xilinx/xstatus.h>
#include <xilinx/xllfifo.h>

int fifo_init(XLlFifo *fifo, char *baseaddr, char *axi_baseaddr);

ssize_t fifo_write(XLlFifo *fifo, char *buf, size_t len, int irq);

ssize_t fifo_read(XLlFifo *fifo, char *buf, size_t len, int irq);

#endif /* _FIFO_H_ */