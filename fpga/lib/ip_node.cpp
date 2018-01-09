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

	json_t* json_ports = json_object_get(json_ip, "ports");
	if(json_ports == nullptr) {
		cpp_error << "IpNode " << ip << " has no ports property";
		return false;
	}

	json_t* json_master = json_object_get(json_ports, "master");
	json_t* json_slave = json_object_get(json_ports, "slave");

	const bool hasMasterPorts = json_is_array(json_master);
	const bool hasSlavePorts = json_is_array(json_slave);

	if( (not hasMasterPorts) and (not hasSlavePorts)) {
		cpp_error << "IpNode " << ip << " has no ports";
		return false;
	}

	// intentionally use short-circuit evaluation to only call populatePorts
	// if the property exists
	bool masterPortsSuccess =
	        (hasMasterPorts and populatePorts(ipNode.portsMaster, json_master))
	        or (not hasMasterPorts);

	bool slavePortsSuccess =
	        (hasSlavePorts and populatePorts(ipNode.portsSlave, json_slave))
	        or (not hasSlavePorts);

	return (masterPortsSuccess and slavePortsSuccess);
}

bool
IpNodeFactory::populatePorts(std::map<int, IpNode::StreamPort>& portMap, json_t* json)
{
	size_t index;
	json_t* json_port;
	json_array_foreach(json, index, json_port) {
		int myPortNum;
		const char* to = nullptr;

		int ret = json_unpack(json_port, "{ s : i, s? : s}",
		                                 "num", &myPortNum,
		                                 "to", &to);
		if(ret != 0) {
			cpp_error << "Port definition required field 'num'";
			return false;
		}

		if(to == nullptr) {
			cpp_warn << "Nothing connected to port " << myPortNum;
			portMap[myPortNum] = {};
			continue;
		}

		const auto tokens = utils::tokenize(to, ":");
		if(tokens.size() != 2) {
			cpp_error << "Too many tokens in property 'other'";
			return false;
		}

		int otherPortNum;
		try {
			otherPortNum = std::stoi(tokens[1]);
		} catch(const std::invalid_argument&) {
			cpp_error << "Other port number is not an integral number";
			return false;
		}

		cpp_debug << "Adding port mapping: " << myPortNum << ":" << to;
		portMap[myPortNum] = { otherPortNum, tokens[0] };
	}

	return true;
}

std::pair<int, int>
IpNode::getLoopbackPorts() const
{
	for(auto& [masterNum, masterTo] : portsMaster) {
		for(auto& [slaveNum, slaveTo] : portsSlave) {
			// TODO: should we also check which IP both ports are connected to?
			if(masterTo.nodeName == slaveTo.nodeName) {
				return { masterNum, slaveNum };
			}
		}
	}

	return { -1, -1 };
}

bool
IpNode::loopbackPossible() const
{
	auto ports = getLoopbackPorts();
	return (ports.first != -1) and (ports.second != -1);
}

bool
IpNode::connectLoopback()
{
	auto ports = getLoopbackPorts();
	const auto& portMaster = portsMaster[ports.first];
	const auto& portSlave = portsSlave[ports.second];

	// TODO: verify this is really a switch!
	auto axiStreamSwitch = reinterpret_cast<ip::AxiStreamSwitch*>(
	                            card->lookupIp(portMaster.nodeName));

	if(axiStreamSwitch == nullptr) {
		cpp_error << "Cannot find IP " << *axiStreamSwitch;
		return false;
	}

	// switch's slave port is our master port and vice versa
	return axiStreamSwitch->connect(portMaster.portNumber, portSlave.portNumber);
}

} // namespace ip
} // namespace fpga
} // namespace villas
