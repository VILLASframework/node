/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <cstdint>

#include <xilinx/xtmrctr.h>

#include <villas/fpga/ips/timer.hpp>
#include <villas/fpga/ips/intc.hpp>

using namespace villas::fpga::ip;

bool Timer::init()
{
	XTmrCtr_Config xtmr_cfg;
	xtmr_cfg.SysClockFreqHz = getFrequency();

	XTmrCtr_CfgInitialize(&xTmr, &xtmr_cfg, getBaseAddr(registerMemory));
	XTmrCtr_InitHw(&xTmr);

	if (irqs.find(irqName) == irqs.end()) {
		logger->error("IRQ '{}' not found but required", irqName);
		return false;
	}

	// Disable so we don't receive any stray interrupts
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

static char n[] = "timer";
static char d[] = "Xilinx's programmable timer / counter";
static char v[] = "xilinx.com:ip:axi_timer:";
static CorePlugin<Timer, n, d, v> f;
