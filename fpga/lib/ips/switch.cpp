/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
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

#include <xilinx/xaxis_switch.h>

#include "log.hpp"
#include "fpga/ips/switch.hpp"

namespace villas {
namespace fpga {
namespace ip {

static AxiStreamSwitchFactory factory;

bool
AxiStreamSwitch::start()
{
	/* Setup AXI-stream switch */
	XAxis_Switch_Config sw_cfg;
	sw_cfg.MaxNumMI = portsMaster.size();
	sw_cfg.MaxNumSI = portsSlave.size();

	if(XAxisScr_CfgInitialize(&xilinxDriver, &sw_cfg, getBaseaddr()) != XST_SUCCESS) {
		cpp_error << "Cannot start " << *this;
		return false;
	}

	/* Disable all masters */
	XAxisScr_RegUpdateDisable(&xilinxDriver);
	XAxisScr_MiPortDisableAll(&xilinxDriver);
	XAxisScr_RegUpdateEnable(&xilinxDriver);

	// initialize internal mapping
	for(int portMaster = 0; portMaster < portsMaster.size(); portMaster++) {
		portMapping[portMaster] = PORT_DISABLED;
	}


	return true;
}

bool
AxiStreamSwitch::connect(int portSlave, int portMaster)
{
	if(portMapping[portMaster] == portSlave) {
		cpp_debug << "Ports already connected";
		return true;
	}

	for(auto [master, slave] : portMapping) {
		if(slave == portSlave) {
			cpp_warn << "Slave " << slave << " has already been connected to "
			         << "master " << master << ". Disabling master " << master;

			XAxisScr_RegUpdateDisable(&xilinxDriver);
			XAxisScr_MiPortDisable(&xilinxDriver, master);
			XAxisScr_RegUpdateEnable(&xilinxDriver);
		}
	}

	/* Reconfigure switch */
	XAxisScr_RegUpdateDisable(&xilinxDriver);
	XAxisScr_MiPortEnable(&xilinxDriver, portMaster, portSlave);
	XAxisScr_RegUpdateEnable(&xilinxDriver);

	cpp_debug << "Connect slave " << portSlave << " to master " << portMaster;

	return true;
}

bool
AxiStreamSwitch::disconnectMaster(int port)
{
	cpp_debug << "Disconnect slave " << portMapping[port]
	          << " from master " << port;

	XAxisScr_MiPortDisable(&xilinxDriver, port);
	portMapping[port] = PORT_DISABLED;
	return true;
}

bool
AxiStreamSwitch::disconnectSlave(int port)
{
	for(auto [master, slave] : portMapping) {
		if(slave == port) {
			cpp_debug << "Disconnect slave " << slave << " from master " << master;
			XAxisScr_MiPortDisable(&xilinxDriver, master);
			return true;
		}
	}

	cpp_debug << "Slave " << port << " hasn't been connected to any master";
	return true;
}

} // namespace ip
} // namespace fpga
} // namespace villas
