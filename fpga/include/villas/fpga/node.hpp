/** Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * @file
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017-2022, Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

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
	    nodeName(node),
	    portName(port),
	    isMaster(isMaster)
	{ }

	std::string getName() const
	{
		return nodeName + "/" + portName + "(" + (isMaster ? "M" : "S") + ")";
	}

	friend
	std::ostream& operator<< (std::ostream &stream, const StreamVertex &vertex)
	{
		return stream << vertex.getIdentifier() << ": " << vertex.getName();
	}

public:
	std::string nodeName;
	std::string portName;
	bool isMaster;
};

class StreamGraph : public graph::DirectedGraph<StreamVertex> {
public:
	StreamGraph() :
		graph::DirectedGraph<StreamVertex>("stream:graph")
	{ }

	std::shared_ptr<StreamVertex> getOrCreateStreamVertex(const std::string &node,
	        const std::string &port,
	        bool isMaster)
	{
		for (auto &vertexEntry : vertices) {
			auto &vertex = vertexEntry.second;
			if (vertex->nodeName == node and vertex->portName == port and vertex->isMaster == isMaster)
				return vertex;
		}

		// Vertex not found, create new one
		auto vertex = std::make_shared<StreamVertex>(node, port, isMaster);
		addVertex(vertex);

		return vertex;
	}
};

class Node : public virtual Core {
public:

	using Ptr = std::shared_ptr<Node>;

	friend class NodeFactory;

	const StreamVertex& getMasterPort(const std::string &name) const
	{
		return *portsMaster.at(name);
	}

	const StreamVertex& getSlavePort(const std::string &name) const
	{
		return *portsSlave.at(name);
	}

	bool connect(const StreamVertex &from, const StreamVertex &to);
	bool connect(const StreamVertex &from, const StreamVertex &to, bool reverse)
	{
		bool ret;

		ret = connect(from, to);

		if (reverse)
			ret &= connect(to, from);

		return ret;
	}

	// Easy-usage assuming that the slave IP to connect to only has one slave
	// port and implements the getDefaultSlavePort() function
	bool connect(const Node &slaveNode, bool reverse = false)
	{
		return this->connect(this->getDefaultMasterPort(), slaveNode.getDefaultSlavePort(), reverse);
	}

	// Used by easy-usage connect, will throw if not implemented by derived node
	virtual
	const StreamVertex& getDefaultSlavePort() const;

	// Used by easy-usage connect, will throw if not implemented by derived node
	virtual
	const StreamVertex& getDefaultMasterPort() const;

	static
	const StreamGraph& getGraph()
	{
		return streamGraph;
	}

	bool loopbackPossible() const;
	bool connectLoopback();

protected:
	virtual
	bool connectInternal(const std::string &slavePort,
	                const std::string &masterPort);

private:
	std::pair<std::string, std::string> getLoopbackPorts() const;

protected:
	std::map<std::string, std::shared_ptr<StreamVertex>> portsMaster;
	std::map<std::string, std::shared_ptr<StreamVertex>> portsSlave;

	static
	StreamGraph streamGraph;
};

class NodeFactory : public CoreFactory {
public:
	using CoreFactory::CoreFactory;

	virtual
	void parse(Core &, json_t *);
};


template<typename T, const char *name, const char *desc, const char *vlnv>
class NodePlugin : public NodeFactory {

public:
virtual
	std::string getName() const
	{
		return name;
	}

	virtual
	std::string getDescription() const
	{
		return desc;
	}

private:
	// Get a VLNV identifier for which this IP / Node type can be used.
	virtual
	Vlnv getCompatibleVlnv() const
	{
		return Vlnv(vlnv);
	}

	// Create a concrete IP instance
	Core* make() const
	{
		return new T;
	}
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
