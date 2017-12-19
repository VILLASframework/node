/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
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


#include "config.h"

#include "log.hpp"
#include "fpga/ips/timer.hpp"

namespace villas {
namespace fpga {
namespace ip {


// instantiate factory to make available to plugin infrastructure
static TimerFactory factory;

bool Timer::start()
{
	XTmrCtr_Config xtmr_cfg;
	xtmr_cfg.SysClockFreqHz = FPGA_AXI_HZ;

	XTmrCtr_CfgInitialize(&xtmr, &xtmr_cfg, getBaseaddr());
	XTmrCtr_InitHw(&xtmr);

	return true;
}


} // namespace ip
} // namespace fpga
} // namespace villas
