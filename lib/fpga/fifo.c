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
#include "fpga/ip.h"
#include "fpga/fifo.h"
#include "fpga/intc.h"

int fifo_init(struct ip *c)
{
	XLlFifo *fifo = &c->fifo_mm;

	int ret;

	XLlFifo_Config fifo_cfg = {
		.BaseAddress = (uintptr_t) c->card->map + c->baseaddr,
		.Axi4BaseAddress = (uintptr_t) c->card->map + c->baseaddr_axi4,
		.Datainterface = ((c->baseaddr_axi4) && (c->baseaddr_axi4 != -1)) ? 1 : 0 /* use AXI4 for Data, AXI4-Lite for control */
	};
	
	ret = XLlFifo_CfgInitialize(fifo, &fifo_cfg, (uintptr_t) c->card->map + c->baseaddr);
	if (ret != XST_SUCCESS)
		return -1;

	XLlFifo_IntEnable(fifo, XLLF_INT_RC_MASK); /* Receive complete IRQ */

	return 0;
}

ssize_t fifo_write(struct ip *c, char *buf, size_t len)
{
	XLlFifo *fifo = &c->fifo_mm;

	XLlFifo_Write(fifo, buf, len);
	XLlFifo_TxSetLen(fifo, len);

	return len;
}

ssize_t fifo_read(struct ip *c, char *buf, size_t len)
{
	XLlFifo *fifo = &c->fifo_mm;

	size_t nextlen = 0;

	while (XLlFifo_RxOccupancy(fifo) == 0) {
		if (c->irq >= 0)
			intc_wait(c->card, c->irq);
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
