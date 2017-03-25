/** FIFO related helper functions
 *
 * These functions present a simpler interface to Xilinx' FIFO driver (XLlFifo_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

#include <unistd.h>

#include "utils.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/fifo.h"
#include "fpga/ips/intc.h"

int fifo_init(struct fpga_ip *c)
{
	int ret;
	
	struct fpga_card *f = c->card;
	struct fifo *fifo = (struct fifo *) &c->_vd;
	
	XLlFifo *xfifo = &fifo->inst;
	XLlFifo_Config fifo_cfg = {
		.BaseAddress = (uintptr_t) f->map + c->baseaddr,
		.Axi4BaseAddress = (uintptr_t) c->card->map + fifo->baseaddr_axi4,
		.Datainterface = (fifo->baseaddr_axi4 != -1) ? 1 : 0 /* use AXI4 for Data, AXI4-Lite for control */
	};
	
	ret = XLlFifo_CfgInitialize(xfifo, &fifo_cfg, (uintptr_t) c->card->map + c->baseaddr);
	if (ret != XST_SUCCESS)
		return -1;

	XLlFifo_IntEnable(xfifo, XLLF_INT_RC_MASK); /* Receive complete IRQ */

	return 0;
}

ssize_t fifo_write(struct fpga_ip *c, char *buf, size_t len)
{
	struct fifo *fifo = (struct fifo *) &c->_vd;

	XLlFifo *xllfifo = &fifo->inst;

	uint32_t tdfv;

	tdfv = XLlFifo_TxVacancy(xllfifo);
	if (tdfv < len)
		return -1;

	XLlFifo_Write(xllfifo, buf, len);
	XLlFifo_TxSetLen(xllfifo, len);

	return len;
}

ssize_t fifo_read(struct fpga_ip *c, char *buf, size_t len)
{
	struct fifo *fifo = (struct fifo *) &c->_vd;

	XLlFifo *xllfifo = &fifo->inst;

	size_t nextlen = 0;
	uint32_t rxlen;

	while (!XLlFifo_IsRxDone(xllfifo))
		intc_wait(c->card->intc, c->irq);
	XLlFifo_IntClear(xllfifo, XLLF_INT_RC_MASK);

	/* Get length of next frame */
	rxlen = XLlFifo_RxGetLen(xllfifo);
	nextlen = MIN(rxlen, len);
	
	/* Read from FIFO */
	XLlFifo_Read(xllfifo, buf, nextlen);

	return nextlen;
}

int fifo_parse(struct fpga_ip *c)
{
	struct fifo *fifo = (struct fifo *) &c->_vd;

	int baseaddr_axi4;
	
	if (config_setting_lookup_int(c->cfg, "baseaddr_axi4", &baseaddr_axi4))
		fifo->baseaddr_axi4 = baseaddr_axi4;
	else
		fifo->baseaddr_axi4 = -1;

	return 0;
}

int fifo_reset(struct fpga_ip *c)
{
	struct fifo *fifo = (struct fifo *) &c->_vd;
	
	XLlFifo_Reset(&fifo->inst);

	return 0;
}

static struct plugin p = {
	.name		= "Xilinx's AXI4 FIFO data mover",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv	= { "xilinx.com", "ip", "axi_fifo_mm_s", NULL },
		.type	= FPGA_IP_TYPE_DATAMOVER,
		.init	= fifo_init,
		.parse	= fifo_parse,
		.reset	= fifo_reset,
		.size	= sizeof(struct fifo)
	}
};

REGISTER_PLUGIN(&p)