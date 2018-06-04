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


StreamGraph streamGraph;

bool
IpNodeFactory::configureJson(IpCore& ip, json_t* json_ip)
{
	auto& ipNode = reinterpret_cast<IpNode&>(ip);
	auto logger = getLogger();

	json_t* json_ports = json_object_get(json_ip, "ports");
	if(not json_is_array(json_ports)) {
		logger->debug("IP has no ports");
		return true;
	}

	size_t index;
	json_t* json_port;
	json_array_foreach(json_ports, index, json_port) {
		if(not json_is_object(json_port)) {
			logger->error("Port {} is not an object", index);
			return false;
		}

		const char *role_raw, *target_raw, *name_raw;
		int ret = json_unpack(json_port, "{ s: s, s: s, s: s }",
		                           "role", &role_raw,
		                           "target", &target_raw,
		                           "name", &name_raw);
		if(ret != 0) {
			logger->error("Cannot parse port {}", index);
			return false;
		}

		const auto tokens = utils::tokenize(target_raw, ":");
		if(tokens.size() != 2) {
			logger->error("Cannot parse 'target' of port {}", index);
			return false;
		}

		const std::string role(role_raw);
		const bool isMaster = (role == "master" or role == "initiator");


		auto thisVertex = streamGraph.getOrCreateStreamVertex(
		                      ip.getInstanceName(),
		                      name_raw,
		                      isMaster);

		auto connectedVertex = streamGraph.getOrCreateStreamVertex(
		                           tokens[0],
		                           tokens[1],
		                           not isMaster);


		if(isMaster) {
			streamGraph.addDefaultEdge(thisVertex->getIdentifier(),
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
	for(auto& [masterName, masterTo] : portsMaster) {
		for(auto& [slaveName, slaveTo] : portsSlave) {
			// TODO: should we also check which IP both ports are connected to?
			if(masterTo->nodeName == slaveTo->nodeName) {
				return { masterName, slaveName };
			}
		}
	}

	return { "", "" };
}

bool IpNode::connect(const StreamVertex& from, const StreamVertex& to)
{
	StreamGraph::Path path;
	if(not streamGraph.getPath(from.getIdentifier(), to.getIdentifier(), path)) {
		logger->error("No path from {} to {}", from, to);
		return false;
	}

	if(path.size() == 0) {
		return true;
	}

	auto currentEdge = path.begin();
	auto firstEdge = streamGraph.getEdge(*currentEdge);
	auto firstHopNode = streamGraph.getVertex(firstEdge->getVertexTo());

	auto nextHopNode = firstHopNode;

	// check if next hop is an internal connection
	if(firstHopNode->nodeName == getInstanceName()) {

		if(not connectInternal(from.portName, firstHopNode->portName)) {
			logger->error("Making internal connection from {} to {} failed",
			              from, *firstHopNode);
			return false;
		}

		// we have to advance to next hop
		if(++currentEdge == path.end()) {
			// arrived at the end of path
			return true;
		}

		auto secondEdge = streamGraph.getEdge(*currentEdge);
		auto secondHopNode = streamGraph.getVertex(secondEdge->getVertexTo());
		nextHopNode = secondHopNode;
	}

	auto nextHopNodeIp = reinterpret_cast<IpNode*>
	                     (card->lookupIp(nextHopNode->nodeName));

	if(nextHopNodeIp == nullptr) {
		logger->error("Cannot find IP {}, this shouldn't happen!",
		              nextHopNode->nodeName);
		return false;
	}

	return nextHopNodeIp->connect(*nextHopNode, to);
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

	logger->debug("switch at: {}", portMaster->nodeName);

	// TODO: verify this is really a switch!
	auto axiStreamSwitch = reinterpret_cast<ip::AxiStreamSwitch*>(
	                            card->lookupIp(portMaster->nodeName));

	if(axiStreamSwitch == nullptr) {
		logger->error("Cannot find switch");
		return false;
	}

	// switch's slave port is our master port and vice versa
	return axiStreamSwitch->connect(*portMaster, *portSlave);
}

} // namespace ip
} // namespace fpga
} // namespace villas
