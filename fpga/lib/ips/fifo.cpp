/* FIFO related helper functions
 *
 * These functions present a simpler interface to Xilinx' FIFO driver (XLlFifo_*)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>

#include <xilinx/xstatus.h>
#include <xilinx/xllfifo.h>

#include <villas/fpga/ips/fifo.hpp>
#include <villas/fpga/ips/intc.hpp>

using namespace villas::fpga::ip;

bool Fifo::init()
{
	XLlFifo_Config fifo_cfg;

	try {
		// If this throws an exception, then there's no AXI4 data interface
		fifo_cfg.Axi4BaseAddress = getBaseAddr(axi4Memory);
		fifo_cfg.Datainterface = 1;
	} catch (const std::out_of_range&) {
		fifo_cfg.Datainterface = 0;
	}

	if (XLlFifo_CfgInitialize(&xFifo, &fifo_cfg, getBaseAddr(registerMemory)) != XST_SUCCESS)
		return false;

	if (irqs.find(irqName) == irqs.end()) {
		logger->error("IRQ '{}' not found but required", irqName);
		return false;
	}

	// Receive complete IRQ
	XLlFifo_IntEnable(&xFifo, XLLF_INT_RC_MASK);
	irqs[irqName].irqController->enableInterrupt(irqs[irqName], false);

	return true;
}

bool Fifo::stop()
{
	// Receive complete IRQ
	XLlFifo_IntDisable(&xFifo, XLLF_INT_RC_MASK);
	irqs[irqName].irqController->disableInterrupt(irqs[irqName]);

	return true;
}

size_t Fifo::write(const void *buf, size_t len)
{

	uint32_t tdfv;

	tdfv = XLlFifo_TxVacancy(&xFifo);
	if (tdfv < len)
		return -1;

	// Buf has to be re-casted because Xilinx driver doesn't use const
	XLlFifo_Write(&xFifo, (void*) buf, len);
	XLlFifo_TxSetLen(&xFifo, len);

	return len;
}

size_t Fifo::read(void *buf, size_t len)
{
	size_t nextlen = 0;
	size_t rxlen;

	while (!XLlFifo_IsRxDone(&xFifo))
		irqs[irqName].irqController->waitForInterrupt(irqs[irqName]);

	XLlFifo_IntClear(&xFifo, XLLF_INT_RC_MASK);

	// Get length of next frame
	rxlen = XLlFifo_RxGetLen(&xFifo);
	nextlen = std::min(rxlen, len);

	// Read from FIFO
	XLlFifo_Read(&xFifo, buf, nextlen);

	return nextlen;
}

static char n1[] = "fifo";
static char d1[] = "Xilinx's AXI4 FIFO data mover";
static char v1[] = "xilinx.com:ip:axi_fifo_mm_s:";
static NodePlugin<Fifo, n1, d1, v1> p1;

static char n2[] = "fifo_data";
static char d2[] = "Xilinx's AXI4 data stream FIFO";
static char v2[] = "xilinx.com:ip:axis_data_fifo:";
static NodePlugin<FifoData, n2, d2, v2> p2;
