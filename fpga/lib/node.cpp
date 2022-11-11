/** An IP node.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/exceptions.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/node.hpp>
#include <villas/fpga/ips/switch.hpp>

using namespace villas::fpga::ip;

StreamGraph
Node::streamGraph;

void NodeFactory::configure(Core &ip, json_t *cfg)
{
	CoreFactory::configure(ip, cfg);

	auto &Node = dynamic_cast<ip::Node&>(ip);
	auto logger = getLogger();

	json_t* json_ports = json_object_get(cfg, "ports");
	if (not json_is_array(json_ports))
		throw ConfigError(json_ports, "", "IP port list must be an array");

	size_t index;
	json_t *json_port;
	json_array_foreach(json_ports, index, json_port) {
		if (not json_is_object(json_port))
			throw ConfigError(json_port, "", "Port {} is not an object", index);

		const char *role_raw, *target_raw, *name_raw;

		json_error_t err;
		int ret = json_unpack_ex(json_port, &err, 0, "{ s: s, s: s, s: s }",
		        "role", &role_raw,
		        "target", &target_raw,
		        "name", &name_raw);
		if (ret != 0)
			throw ConfigError(json_port, err, "", "Cannot parse port {}", index);

		const auto tokens = utils::tokenize(target_raw, ":");
		if (tokens.size() != 2)
			throw ConfigError(json_port, err, "", "Cannot parse 'target' of port {}", index);

		const std::string role(role_raw);
		const bool isMaster = (role == "master" or role == "initiator");

		auto thisVertex = Node::streamGraph.getOrCreateStreamVertex(
		                      ip.getInstanceName(),
		                      name_raw,
		                      isMaster);

		auto connectedVertex = Node::streamGraph.getOrCreateStreamVertex(
		                           tokens[0],
		                           tokens[1],
		                           not isMaster);

		if (isMaster) {
			Node::streamGraph.addDefaultEdge(thisVertex->getIdentifier(),
			                                   connectedVertex->getIdentifier());
			Node.portsMaster[name_raw] = thisVertex;
		}
		else // Slave
			Node.portsSlave[name_raw] = thisVertex;
	}
}

std::pair<std::string, std::string>
Node::getLoopbackPorts() const
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

bool Node::connect(const StreamVertex &from, const StreamVertex &to)
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

	// Check if next hop is an internal connection
	if (firstHopNode->nodeName == getInstanceName()) {
		if (not connectInternal(from.portName, firstHopNode->portName)) {
			logger->error("Making internal connection from {} to {} failed",
			              from, *firstHopNode);
			return false;
		}

		// We have to advance to next hop
		if (++currentEdge == path.end())
			return true; // Arrived at the end of path

		auto secondEdge = streamGraph.getEdge(*currentEdge);
		auto secondHopNode = streamGraph.getVertex(secondEdge->getVertexTo());
		nextHopNode = secondHopNode;
	}

	auto nextHopNodeIp = std::dynamic_pointer_cast<Node>
	                     (card->lookupIp(nextHopNode->nodeName));

	if (nextHopNodeIp == nullptr) {
		logger->error("Cannot find IP {}, this shouldn't happen!",
		              nextHopNode->nodeName);
		return false;
	}

	return nextHopNodeIp->connect(*nextHopNode, to);
}

const StreamVertex&
Node::getDefaultSlavePort() const
{
	logger->error("No default slave port available");
	throw std::exception();
}

const StreamVertex&
Node::getDefaultMasterPort() const
{
	logger->error("No default master port available");
	throw std::exception();
}

bool
Node::loopbackPossible() const
{
	auto ports = getLoopbackPorts();
	return (not ports.first.empty()) and (not ports.second.empty());
}

bool
Node::connectInternal(const std::string &slavePort,
                        const std::string &masterPort)
{
	(void) slavePort;
	(void) masterPort;

	logger->warn("This IP doesn't implement an internal connection");
	return false;
}

bool
Node::connectLoopback()
{
	auto ports = getLoopbackPorts();
	const auto &portMaster = portsMaster[ports.first];
	const auto &portSlave = portsSlave[ports.second];

	logger->debug("master port: {}", ports.first);
	logger->debug("slave port: {}", ports.second);

	return connect(*portMaster, *portSlave);
}
