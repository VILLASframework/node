#include <map>
#include <stdexcept>
#include <jansson.h>

#include "utils.hpp"
#include "fpga/ip_node.hpp"
#include "fpga/ips/switch.hpp"
#include "fpga/card.hpp"

namespace villas {
namespace fpga {
namespace ip {


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

		int port_num;
		try {
			port_num = std::stoi(tokens[1]);
		} catch(const std::invalid_argument&) {
			logger->error("Target port number is not an integer: '{}'", target_raw);
			return false;
		}

		IpNode::StreamPort port;
		port.nodeName = tokens[0];
		port.portNumber = port_num;

		const std::string role(role_raw);
		if(role == "master" or role == "initiator") {
			ipNode.portsMaster[name_raw] = port;
		} else /* slave */ {
			ipNode.portsSlave[name_raw] = port;
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
			if(masterTo.nodeName == slaveTo.nodeName) {
				return { masterName, slaveName };
			}
		}
	}

	return { "", "" };
}

bool
IpNode::loopbackPossible() const
{
	auto ports = getLoopbackPorts();
	return (not ports.first.empty()) and (not ports.second.empty());
}

bool
IpNode::connectLoopback()
{
	auto logger = getLogger();

	auto ports = getLoopbackPorts();
	const auto& portMaster = portsMaster[ports.first];
	const auto& portSlave = portsSlave[ports.second];

	logger->debug("master port: {}", ports.first);
	logger->debug("slave port: {}", ports.second);

	logger->debug("switch at: {}", portMaster.nodeName);

	// TODO: verify this is really a switch!
	auto axiStreamSwitch = reinterpret_cast<ip::AxiStreamSwitch*>(
	                            card->lookupIp(portMaster.nodeName));

	if(axiStreamSwitch == nullptr) {
		logger->error("Cannot find switch");
		return false;
	}

	// switch's slave port is our master port and vice versa
	return axiStreamSwitch->connect(portMaster.portNumber, portSlave.portNumber);
}

} // namespace ip
} // namespace fpga
} // namespace villas
