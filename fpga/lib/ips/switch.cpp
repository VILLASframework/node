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

#include <villas/fpga/ips/switch.hpp>

namespace villas {
namespace fpga {
namespace ip {

static AxiStreamSwitchFactory factory;

bool
AxiStreamSwitch::init()
{	
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

	for(auto& [masterName, masterPort] : portsMaster) {

		// initialize internal mapping
		portMapping[masterName] = PORT_DISABLED;

		// each slave port may be internally routed to a master port
		for(auto& [slaveName, slavePort] : portsSlave) {
			(void) slaveName;

			streamGraph.addDefaultEdge(slavePort->getIdentifier(),
			                           masterPort->getIdentifier());
		}
	}

	return true;
}

bool
AxiStreamSwitch::connectInternal(const std::string& portSlave,
                                 const std::string& portMaster)
{
	// check if slave port exists
	try {
		getSlavePort(portSlave);
	} catch(const std::out_of_range&) {
		logger->error("Switch doesn't have a slave port named '{}'", portSlave);
		return false;
	}

	// check if master port exists
	try {
		getMasterPort(portMaster);
	} catch(const std::out_of_range&) {
		logger->error("Switch doesn't have a master port named '{}'", portMaster);
		return false;
	}

	if(portSlave.substr(0, 1) != "S" or
	   portMaster.substr(0, 1) != "M") {
		logger->error("sanity check failed: master {} slave {}",
		              portMaster, portSlave);
		return false;
	}

	if(portMapping[portMaster] == portSlave) {
		logger->debug("Ports already connected (slave {} to master {}",
		              portSlave, portMaster);
		return true;
	}

	for(auto [master, slave] : portMapping) {
		if(slave == portSlave) {
			logger->warn("Slave {} has already been connected to master {}. "
			             "Disabling master {}.",
			             slave, master, master);

			XAxisScr_RegUpdateDisable(&xSwitch);
			XAxisScr_MiPortDisable(&xSwitch, portNameToNum(master));
			XAxisScr_RegUpdateEnable(&xSwitch);

			portMapping[master] = PORT_DISABLED;
		}
	}

	/* Reconfigure switch */
	XAxisScr_RegUpdateDisable(&xSwitch);
	XAxisScr_MiPortEnable(&xSwitch, portNameToNum(portMaster), portNameToNum(portSlave));
	XAxisScr_RegUpdateEnable(&xSwitch);

	portMapping[portMaster] = portSlave;

	logger->debug("Connect slave {} to master {}", portSlave, portMaster);

	return true;
}

int
AxiStreamSwitch::portNameToNum(const std::string& portName)
{
	const std::string number = portName.substr(1, 2);
	return std::stoi(number);
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
