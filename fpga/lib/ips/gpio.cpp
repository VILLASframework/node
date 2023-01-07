/** AXI General Purpose IO (GPIO)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <villas/plugin.hpp>

#include <villas/fpga/ips/gpio.hpp>

using namespace villas::fpga::ip;

bool
Gpio::init()
{
	//const uintptr_t base = getBaseAddr(registerMemory);

	return true;
}

static char n[] = "gpio";
static char d[] = "Xilinx's AXI4 general purpose IO";
static char v[] = "xilinx.com:ip:axi_gpio:";
static CorePlugin<Gpio, n, d, v> f;
