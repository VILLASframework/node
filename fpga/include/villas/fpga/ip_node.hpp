/** Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#ifndef VILLAS_IP_NODE_HPP
#define VILLAS_IP_NODE_HPP

#include <map>
#include <string>
#include <jansson.h>

#include "ip.hpp"
#include "log.hpp"

namespace villas {
namespace fpga {
namespace ip {

// TODO: reflect on interface that an IpNode exposes and how to design it to
//       blend in with VILLASnode software nodes
class IpNode : public IpCore {
public:

	friend class IpNodeFactory;

	struct StreamPort {
		int portNumber;
		std::string nodeName;
	};

	bool connect(std::string portName, const StreamPort& to);
	bool disconnect(std::string portName);

	bool loopbackPossible() const;
	bool connectLoopback();

private:
	std::pair<std::string, std::string> getLoopbackPorts() const;

protected:
	std::map<std::string, StreamPort> portsMaster;
	std::map<std::string, StreamPort> portsSlave;
};

class IpNodeFactory : public IpCoreFactory {
public:
	IpNodeFactory(std::string name) : IpCoreFactory("Ip Node - " + name) {}

	virtual bool configureJson(IpCore& ip, json_t *json_ip);

private:
	bool populatePorts(std::map<int, IpNode::StreamPort>& portMap, json_t* json);
};

/** @} */

} // namespace ip
} // namespace fpga
} // namespace villas

#endif // VILLAS_IP_NODE_HPP
