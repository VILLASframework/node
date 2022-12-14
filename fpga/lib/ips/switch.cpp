/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2022, Steffen Vogel
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

#include <villas/exceptions.hpp>
#include <villas/fpga/ips/switch.hpp>

namespace villas {
namespace fpga {
namespace ip {

static AxiStreamSwitchFactory factory;

bool AxiStreamSwitch::init()
{
	if (XAxisScr_CfgInitialize(&xSwitch, &xConfig, getBaseAddr(registerMemory)) != XST_SUCCESS) {
		logger->error("Cannot initialize switch");
		return false;
	}

	// Disable all masters
	XAxisScr_RegUpdateDisable(&xSwitch);
	XAxisScr_MiPortDisableAll(&xSwitch);
	XAxisScr_RegUpdateEnable(&xSwitch);

	for (auto& [masterName, masterPort] : portsMaster) {

		// Initialize internal mapping
		portMapping[masterName] = PORT_DISABLED;

		// Each slave port may be internally routed to a master port
		for (auto& [slaveName, slavePort] : portsSlave) {
			(void) slaveName;

			streamGraph.addDefaultEdge(slavePort->getIdentifier(),
			                           masterPort->getIdentifier());
		}
	}

	return true;
}

bool AxiStreamSwitch::connectInternal(const std::string &portSlave,
                                 const std::string &portMaster)
{
	// Check if slave port exists
	try {
		getSlavePort(portSlave);
	} catch (const std::out_of_range&) {
		logger->error("Switch doesn't have a slave port named '{}'", portSlave);
		return false;
	}

	// Check if master port exists
	try {
		getMasterPort(portMaster);
	} catch (const std::out_of_range&) {
		logger->error("Switch doesn't have a master port named '{}'", portMaster);
		return false;
	}

	if (portSlave.substr(0, 1) != "S" or
	   portMaster.substr(0, 1) != "M") {
		logger->error("sanity check failed: master {} slave {}",
		              portMaster, portSlave);
		return false;
	}

	if (portMapping[portMaster] == portSlave) {
		logger->debug("Ports already connected (slave {} to master {}",
		              portSlave, portMaster);
		return true;
	}

	for (auto [master, slave] : portMapping) {
		if (slave == portSlave) {
			logger->warn("Slave {} has already been connected to master {}. "
			             "Disabling master {}.",
			             slave, master, master);

			XAxisScr_RegUpdateDisable(&xSwitch);
			XAxisScr_MiPortDisable(&xSwitch, portNameToNum(master));
			XAxisScr_RegUpdateEnable(&xSwitch);

			portMapping[master] = PORT_DISABLED;
		}
	}

	// Reconfigure switch
	XAxisScr_RegUpdateDisable(&xSwitch);
	XAxisScr_MiPortEnable(&xSwitch, portNameToNum(portMaster), portNameToNum(portSlave));
	XAxisScr_RegUpdateEnable(&xSwitch);

	portMapping[portMaster] = portSlave;

	logger->debug("Connect slave {} to master {}", portSlave, portMaster);

	return true;
}

int AxiStreamSwitch::portNameToNum(const std::string &portName)
{
	const std::string number = portName.substr(1, 2);
	return std::stoi(number);
}

void AxiStreamSwitchFactory::parse(Core &ip, json_t *cfg)
{
	NodeFactory::parse(ip, cfg);

	auto logger = getLogger();

	auto &axiSwitch = dynamic_cast<AxiStreamSwitch&>(ip);

	int num_si, num_mi;
	json_error_t err;
	auto ret = json_unpack_ex(cfg, &err, 0, "{ s: { s: i, s: i } }",
		"parameters",
			"num_si", &num_si,
			"num_mi", &num_mi
	);
	if (ret != 0)
		throw ConfigError(cfg, err, "", "Cannot parse switch config");

	axiSwitch.xConfig.MaxNumMI = num_mi;
	axiSwitch.xConfig.MaxNumSI = num_si;
}

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
