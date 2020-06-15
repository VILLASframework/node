/** Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <map>
#include <string>
#include <jansson.h>

#include <villas/fpga/core.hpp>

#include <villas/graph/directed.hpp>

namespace villas {
namespace fpga {
namespace ip {

class StreamVertex : public graph::Vertex {
public:
	StreamVertex(const std::string &node, const std::string &port, bool isMaster) :
	    nodeName(node), portName(port), isMaster(isMaster) {}

	std::string getName() const
	{ return nodeName + "/" + portName + "(" + (isMaster ? "M" : "S") + ")"; }

	friend std::ostream&
	operator<< (std::ostream &stream, const StreamVertex &vertex)
	{ return stream << vertex.getIdentifier() << ": " << vertex.getName(); }

public:
	std::string nodeName;
	std::string portName;
	bool isMaster;
};


class StreamGraph : public graph::DirectedGraph<StreamVertex> {
public:
	StreamGraph() : graph::DirectedGraph<StreamVertex>("StreamGraph") {}

	std::shared_ptr<StreamVertex>
	getOrCreateStreamVertex(const std::string &node,
	                        const std::string &port,
	                        bool isMaster)
	{
		for (auto &vertexEntry : vertices) {
			auto &vertex = vertexEntry.second;
			if (vertex->nodeName == node and vertex->portName == port and vertex->isMaster == isMaster)
				return vertex;
		}

		// vertex not found, create new one
		auto vertex = std::make_shared<StreamVertex>(node, port, isMaster);
		addVertex(vertex);

		return vertex;
	}
};


class Node : public virtual Core {
public:

	using Ptr = std::shared_ptr<Node>;

	friend class NodeFactory;

	struct StreamPort {
		int portNumber;
		std::string nodeName;
	};

	const StreamVertex&
	getMasterPort(const std::string &name) const
	{ return *portsMaster.at(name); }

	const StreamVertex&
	getSlavePort(const std::string &name) const
	{ return *portsSlave.at(name); }

	// easy-usage assuming that the slave IP to connect to only has one slave
	// port and implements the getDefaultSlavePort() function
	bool connect(const Node& slaveNode)
	{ return this->connect(this->getDefaultMasterPort(), slaveNode.getDefaultSlavePort()); }

	// used by easy-usage connect, will throw if not implemented by derived node
	virtual const StreamVertex&
	getDefaultSlavePort() const;

	// used by easy-usage connect, will throw if not implemented by derived node
	virtual const StreamVertex&
	getDefaultMasterPort() const;

	static const StreamGraph&
	getGraph()
	{ return streamGraph; }

	bool loopbackPossible() const;
	bool connectLoopback();

protected:
	virtual bool
	connectInternal(const std::string &slavePort,
	                const std::string &masterPort);

private:
	std::pair<std::string, std::string> getLoopbackPorts() const;

protected:
	std::map<std::string, std::shared_ptr<StreamVertex>> portsMaster;
	std::map<std::string, std::shared_ptr<StreamVertex>> portsSlave;

	static StreamGraph streamGraph;
};

class NodeFactory : public CoreFactory {
public:
	using CoreFactory::CoreFactory;

	virtual bool configureJson(Core &ip, json_t *json_ip);
};

/** @} */

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
