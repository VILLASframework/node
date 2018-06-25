/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <cstdint>

#include <xilinx/xtmrctr.h>

#include <villas/fpga/ips/timer.hpp>
#include <villas/fpga/ips/intc.hpp>

namespace villas {
namespace fpga {
namespace ip {


// instantiate factory to make available to plugin infrastructure
static TimerFactory factory;

bool Timer::init()
{
	XTmrCtr_Config xtmr_cfg;
	xtmr_cfg.SysClockFreqHz = getFrequency();

	XTmrCtr_CfgInitialize(&xTmr, &xtmr_cfg, getBaseAddr(registerMemory));
	XTmrCtr_InitHw(&xTmr);

	if(irqs.find(irqName) == irqs.end()) {
		logger->error("IRQ '{}' not found but required", irqName);
		return false;
	}

	// disable so we don't receive any stray interrupts
	irqs[irqName].irqController->disableInterrupt(irqs[irqName]);

	return true;
}

bool Timer::start(uint32_t ticks)
{
	irqs[irqName].irqController->enableInterrupt(irqs[irqName], false);

	XTmrCtr_SetOptions(&xTmr, 0, XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION);
	XTmrCtr_SetResetValue(&xTmr, 0, ticks);
	XTmrCtr_Start(&xTmr, 0);

	return true;
}

bool Timer::wait()
{
	int count = irqs[irqName].irqController->waitForInterrupt(irqs[irqName]);
	irqs[irqName].irqController->disableInterrupt(irqs[irqName]);

	return (count == 1);
}

uint32_t Timer::remaining()
{
	return XTmrCtr_GetValue(&xTmr, 0);
}


} // namespace ip
} // namespace fpga
} // namespace villas
