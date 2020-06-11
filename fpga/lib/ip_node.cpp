/** An IP node.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <map>
#include <stdexcept>
#include <jansson.h>

#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ip_node.hpp>
#include <villas/fpga/ips/switch.hpp>

namespace villas {
namespace fpga {
namespace ip {


StreamGraph
IpNode::streamGraph;

bool
IpNodeFactory::configureJson(IpCore& ip, json_t* json_ip)
{
	auto& ipNode = dynamic_cast<IpNode&>(ip);
	auto logger = getLogger();

	json_t* json_ports = json_object_get(json_ip, "ports");
	if (not json_is_array(json_ports)) {
		logger->debug("IP has no ports");
		return true;
	}

	size_t index;
	json_t* json_port;
	json_array_foreach(json_ports, index, json_port) {
		if (not json_is_object(json_port)) {
			logger->error("Port {} is not an object", index);
			return false;
		}

		const char *role_raw, *target_raw, *name_raw;
		int ret = json_unpack(json_port, "{ s: s, s: s, s: s }",
		                           "role", &role_raw,
		                           "target", &target_raw,
		                           "name", &name_raw);
		if (ret != 0) {
			logger->error("Cannot parse port {}", index);
			return false;
		}

		const auto tokens = utils::tokenize(target_raw, ":");
		if (tokens.size() != 2) {
			logger->error("Cannot parse 'target' of port {}", index);
			return false;
		}

		const std::string role(role_raw);
		const bool isMaster = (role == "master" or role == "initiator");

		auto thisVertex = IpNode::streamGraph.getOrCreateStreamVertex(
		                      ip.getInstanceName(),
		                      name_raw,
		                      isMaster);

		auto connectedVertex = IpNode::streamGraph.getOrCreateStreamVertex(
		                           tokens[0],
		                           tokens[1],
		                           not isMaster);


		if (isMaster) {
			IpNode::streamGraph.addDefaultEdge(thisVertex->getIdentifier(),
			                                   connectedVertex->getIdentifier());
			ipNode.portsMaster[name_raw] = thisVertex;
		} else /* slave */ {
			ipNode.portsSlave[name_raw] = thisVertex;
		}
	}

	return true;
}

std::pair<std::string, std::string>
IpNode::getLoopbackPorts() const
{
	for (auto& [masterName, masterVertex] : portsMaster) {
		for (auto& [slaveName, slaveVertex] : portsSlave) {
			StreamGraph::Path path;
			if (streamGraph.getPath(masterVertex->getIdentifier(), slaveVertex->getIdentifier(), path)) {
				return { masterName, slaveName };
			}
		}
	}

	return { "", "" };
}

bool IpNode::connect(const StreamVertex& from, const StreamVertex& to)
{
	if (from.nodeName != getInstanceName()) {
		logger->error("Cannot connect from a foreign StreamVertex: {}", from);
		return false;
	}

	StreamGraph::Path path;
	if (not streamGraph.getPath(from.getIdentifier(), to.getIdentifier(), path)) {
		logger->error("No path from {} to {}", from, to);
		return false;
	}

	if (path.size() == 0) {
		return true;
	}

	auto currentEdge = path.begin();
	auto firstEdge = streamGraph.getEdge(*currentEdge);
	auto firstHopNode = streamGraph.getVertex(firstEdge->getVertexTo());

	auto nextHopNode = firstHopNode;

	// check if next hop is an internal connection
	if (firstHopNode->nodeName == getInstanceName()) {

		if (not connectInternal(from.portName, firstHopNode->portName)) {
			logger->error("Making internal connection from {} to {} failed",
			              from, *firstHopNode);
			return false;
		}

		// we have to advance to next hop
		if (++currentEdge == path.end()) {
			// arrived at the end of path
			return true;
		}

		auto secondEdge = streamGraph.getEdge(*currentEdge);
		auto secondHopNode = streamGraph.getVertex(secondEdge->getVertexTo());
		nextHopNode = secondHopNode;
	}

	auto nextHopNodeIp = std::dynamic_pointer_cast<IpNode>
	                     (card->lookupIp(nextHopNode->nodeName));

	if (nextHopNodeIp == nullptr) {
		logger->error("Cannot find IP {}, this shouldn't happen!",
		              nextHopNode->nodeName);
		return false;
	}

	return nextHopNodeIp->connect(*nextHopNode, to);
}

const StreamVertex&
IpNode::getDefaultSlavePort() const
{
	logger->error("No default slave port available");
	throw std::exception();
}

const StreamVertex&
IpNode::getDefaultMasterPort() const
{
	logger->error("No default master port available");
	throw std::exception();
}

bool
IpNode::loopbackPossible() const
{
	auto ports = getLoopbackPorts();
	return (not ports.first.empty()) and (not ports.second.empty());
}

bool
IpNode::connectInternal(const std::string& slavePort,
                        const std::string& masterPort)
{
	(void) slavePort;
	(void) masterPort;

	logger->warn("This IP doesn't implement an internal connection");
	return false;
}

bool
IpNode::connectLoopback()
{
	auto ports = getLoopbackPorts();
	const auto& portMaster = portsMaster[ports.first];
	const auto& portSlave = portsSlave[ports.second];

	logger->debug("master port: {}", ports.first);
	logger->debug("slave port: {}", ports.second);

	return connect(*portMaster, *portSlave);
}

} // namespace ip
} // namespace fpga
} // namespace villas
