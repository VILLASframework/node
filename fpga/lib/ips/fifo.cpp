/** FIFO related helper functions
 *
 * These functions present a simpler interface to Xilinx' FIFO driver (XLlFifo_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <unistd.h>

#include <xilinx/xstatus.h>
#include <xilinx/xllfifo.h>

#include "log.hpp"
#include "fpga/ips/fifo.hpp"
#include "fpga/ips/intc.hpp"


namespace villas {
namespace fpga {
namespace ip {

// instantiate factory to make available to plugin infrastructure
static FifoFactory factory;

bool
FifoFactory::configureJson(IpCore &ip, json_t *json_ip)
{
	auto logger = getLogger();

	if(not IpNodeFactory::configureJson(ip, json_ip)) {
		logger->error("Configuring IpNode failed");
		return false;
	}

	auto& fifo = reinterpret_cast<Fifo&>(ip);
	if(json_unpack(json_ip, "{ s: i }", "axi4_baseaddr", &fifo.baseaddr_axi4) != 0) {
		logger->warn("Cannot parse property 'axi4_baseaddr'");
		return false;
	}

	return true;
}


bool Fifo::init()
{
	XLlFifo_Config fifo_cfg;

	fifo_cfg.Axi4BaseAddress = getAddrMapped(this->baseaddr_axi4);

	// use AXI4 for Data, AXI4-Lite for control
	fifo_cfg.Datainterface = (this->baseaddr_axi4 != static_cast<size_t>(-1)) ? 1 : 0;

	if (XLlFifo_CfgInitialize(&xFifo, &fifo_cfg, getBaseaddr()) != XST_SUCCESS)
		return false;

	// Receive complete IRQ
	XLlFifo_IntEnable(&xFifo, XLLF_INT_RC_MASK);

	auto intc = reinterpret_cast<InterruptController*>(dependencies["intc"]);
	intc->enableInterrupt(irqs["interrupt"], false);

	return true;
}

bool Fifo::stop()
{
	// Receive complete IRQ
	XLlFifo_IntDisable(&xFifo, XLLF_INT_RC_MASK);

	return true;
}

size_t Fifo::write(const void *buf, size_t len)
{

	uint32_t tdfv;

	tdfv = XLlFifo_TxVacancy(&xFifo);
	if (tdfv < len)
		return -1;

	// buf has to be re-casted because Xilinx driver doesn't use const
	XLlFifo_Write(&xFifo, (void*) buf, len);
	XLlFifo_TxSetLen(&xFifo, len);

	return len;
}

size_t Fifo::read(void *buf, size_t len)
{
	size_t nextlen = 0;
	size_t rxlen;

	auto intc = reinterpret_cast<InterruptController*>(dependencies["intc"]);

	while (!XLlFifo_IsRxDone(&xFifo))
		intc->waitForInterrupt(irqs["interrupt"].num);

	XLlFifo_IntClear(&xFifo, XLLF_INT_RC_MASK);

	/* Get length of next frame */
	rxlen = XLlFifo_RxGetLen(&xFifo);
	nextlen = std::min(rxlen, len);

	/* Read from FIFO */
	XLlFifo_Read(&xFifo, buf, nextlen);

	return nextlen;
}

#if 0


ssize_t fifo_write(struct fpga_ip *c, char *buf, size_t len)
{
	struct fifo *fifo = (struct fifo *) c->_vd;

	XLlFifo *xllfifo = &fifo->inst;

}

ssize_t fifo_read(struct fpga_ip *c, char *buf, size_t len)
{
	struct fifo *fifo = (struct fifo *) c->_vd;

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

int fifo_parse(struct fpga_ip *c, json_t *cfg)
{
	struct fifo *fifo = (struct fifo *) c->_vd;

	int baseaddr_axi4 = -1, ret;

	json_error_t err;

	fifo->baseaddr_axi4 = -1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i }", "baseaddr_axi4", &baseaddr_axi4);
	if (ret)
		jerror(&err, "Failed to parse configuration of FPGA IP '%s'", c->name);

	fifo->baseaddr_axi4 = baseaddr_axi4;

	return 0;
}

int fifo_reset(struct fpga_ip *c)
{
	struct fifo *fifo = (struct fifo *) c->_vd;

	XLlFifo_Reset(&fifo->inst);

	return 0;
}
#endif

} // namespace ip
} // namespace fpga
} // namespace villas
