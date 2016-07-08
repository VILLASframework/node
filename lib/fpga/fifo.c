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
	struct fifo *fifo = &c->fifo;
	XLlFifo *xfifo = &fifo->inst;

	int ret;

	XLlFifo_Config fifo_cfg = {
		.BaseAddress = (uintptr_t) c->card->map + c->baseaddr,
		.Axi4BaseAddress = (uintptr_t) c->card->map + fifo->baseaddr_axi4,
		.Datainterface = (fifo->baseaddr_axi4 != -1) ? 1 : 0 /* use AXI4 for Data, AXI4-Lite for control */
	};
	
	ret = XLlFifo_CfgInitialize(xfifo, &fifo_cfg, (uintptr_t) c->card->map + c->baseaddr);
	if (ret != XST_SUCCESS)
		return -1;

	XLlFifo_IntEnable(xfifo, XLLF_INT_RC_MASK); /* Receive complete IRQ */

	return 0;
}

ssize_t fifo_write(struct ip *c, char *buf, size_t len)
{
	XLlFifo *fifo = &c->fifo.inst;
	uint32_t tdfv;

	tdfv = XLlFifo_TxVacancy(fifo);
	if (tdfv < len)
		return -1;

	XLlFifo_Write(fifo, buf, len);
	XLlFifo_TxSetLen(fifo, len);

	return len;
}

ssize_t fifo_read(struct ip *c, char *buf, size_t len)
{
	XLlFifo *fifo = &c->fifo.inst;

	size_t nextlen = 0;
	uint32_t rxlen;

	while (!XLlFifo_IsRxDone(fifo))
		intc_wait(c->card->intc, c->irq);
	XLlFifo_IntClear(fifo, XLLF_INT_RC_MASK);

	/* Get length of next frame */
	rxlen = XLlFifo_RxGetLen(fifo);
	nextlen = MIN(rxlen, len);
	
	/* Read from FIFO */
	XLlFifo_Read(fifo, buf, nextlen);

	return nextlen;
}

int fifo_parse(struct ip *c)
{
	struct fifo *fifo = &c->fifo;

	int baseaddr_axi4;
	
	if (config_setting_lookup_int(c->cfg, "baseaddr_axi4", &baseaddr_axi4))
		fifo->baseaddr_axi4 = baseaddr_axi4;
	else
		fifo->baseaddr_axi4 = -1;

	return 0;
}

int fifo_reset(struct ip *c)
{
	XLlFifo_Reset(&c->fifo.inst);

	return 0;
}

static struct ip_type ip = {
	.vlnv = { "xilinx.com", "ip", "axi_fifo_mm_s", NULL },
	.init = fifo_init,
	.parse = fifo_parse,
	.reset = fifo_reset
};

REGISTER_IP_TYPE(&ip)