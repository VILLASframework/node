/** FIFO related helper functions
 *
 * These functions present a simpler interface to Xilinx' FIFO driver (XLlFifo_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASfpga. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <unistd.h>

#include <villas/utils.h>

#include "utils.h"
#include "fpga/fifo.h"

int fifo_init(XLlFifo *fifo, char *baseaddr, char *axi_baseaddr)
{
	int ret;

	XLlFifo_Config fifo_cfg = {
		.BaseAddress = (uintptr_t) baseaddr,
		.Axi4BaseAddress = (uintptr_t) axi_baseaddr,
		.Datainterface = (axi_baseaddr == NULL) ? 0 : 1 /* use AXI4 for Data, AXI4-Lite for control */
	};
	
	ret = XLlFifo_CfgInitialize(fifo, &fifo_cfg, (uintptr_t) baseaddr);
	if (ret != XST_SUCCESS)
		return -1;

	XLlFifo_IntEnable(fifo, XLLF_INT_RC_MASK);

	return 0;
}

ssize_t fifo_write(XLlFifo *fifo, char *buf, size_t len, int irq)
{
	XLlFifo_Write(fifo, buf, len);
	XLlFifo_TxSetLen(fifo, len);

	return len;
}

ssize_t fifo_read(XLlFifo *fifo, char *buf, size_t len, int irq)
{
	size_t nextlen = 0;

	while (XLlFifo_RxOccupancy(fifo) == 0) {
		if (irq >= 0)
			wait_irq(irq);
		else
			__asm__("nop");
	}

	/* Get length of next frame */
	nextlen = MIN(XLlFifo_RxGetLen(fifo), len);

	/* Read from FIFO */
	XLlFifo_Read(fifo, buf, nextlen);
	
	XLlFifo_IntClear(fifo, XLLF_INT_RC_MASK);

	return nextlen;
}
