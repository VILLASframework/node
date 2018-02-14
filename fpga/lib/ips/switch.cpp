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

#include <jansson.h>
#include <xilinx/xaxis_switch.h>

#include "log.hpp"
#include "fpga/ips/switch.hpp"

namespace villas {
namespace fpga {
namespace ip {

static AxiStreamSwitchFactory factory;

bool
AxiStreamSwitch::init()
{	
	auto logger = getLogger();

	/* Setup AXI-stream switch */
	XAxis_Switch_Config sw_cfg;
	sw_cfg.MaxNumMI = num_ports;
	sw_cfg.MaxNumSI = num_ports;

	if(XAxisScr_CfgInitialize(&xSwitch, &sw_cfg, getBaseAddr(registerMemory)) != XST_SUCCESS) {
		logger->error("Cannot initialize switch");
		return false;
	}

	/* Disable all masters */
	XAxisScr_RegUpdateDisable(&xSwitch);
	XAxisScr_MiPortDisableAll(&xSwitch);
	XAxisScr_RegUpdateEnable(&xSwitch);

	// initialize internal mapping
	for(size_t portMaster = 0; portMaster < portsMaster.size(); portMaster++) {
		portMapping[portMaster] = PORT_DISABLED;
	}

	return true;
}

bool
AxiStreamSwitch::connect(int portSlave, int portMaster)
{
	auto logger = getLogger();

	if(portMapping[portMaster] == portSlave) {
		logger->debug("Ports already connected");
		return true;
	}

	for(auto [master, slave] : portMapping) {
		if(slave == portSlave) {
			logger->warn("Slave {} has already been connected to master {}. "
			             "Disabling master {}.",
			             slave, master, master);

			XAxisScr_RegUpdateDisable(&xSwitch);
			XAxisScr_MiPortDisable(&xSwitch, master);
			XAxisScr_RegUpdateEnable(&xSwitch);
		}
	}

	/* Reconfigure switch */
	XAxisScr_RegUpdateDisable(&xSwitch);
	XAxisScr_MiPortEnable(&xSwitch, portMaster, portSlave);
	XAxisScr_RegUpdateEnable(&xSwitch);

	logger->debug("Connect slave {} to master {}", portSlave, portMaster);

	return true;
}

bool
AxiStreamSwitch::disconnectMaster(int port)
{
	auto logger = getLogger();

	logger->debug("Disconnect slave {} from master {}",
	              portMapping[port], port);

	XAxisScr_MiPortDisable(&xSwitch, port);
	portMapping[port] = PORT_DISABLED;
	return true;
}

bool
AxiStreamSwitch::disconnectSlave(int port)
{
	auto logger = getLogger();

	for(auto [master, slave] : portMapping) {
		if(slave == port) {
			logger->debug("Disconnect slave {} from master {}", slave, master);
			XAxisScr_MiPortDisable(&xSwitch, master);
			return true;
		}
	}

	logger->debug("Slave {} hasn't been connected to any master", port);
	return true;
}

bool
AxiStreamSwitchFactory::configureJson(IpCore& ip, json_t* json_ip)
{
	if(not IpNodeFactory::configureJson(ip, json_ip))
		return false;

	auto logger = getLogger();

	auto& axiSwitch = reinterpret_cast<AxiStreamSwitch&>(ip);

	if(json_unpack(json_ip, "{ s: i }", "num_ports", &axiSwitch.num_ports) != 0) {
		logger->error("Cannot parse 'num_ports'");
		return false;
	}

	return true;
}


} // namespace ip
} // namespace fpga
} // namespace villas
