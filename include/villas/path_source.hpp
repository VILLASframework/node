/* Message source
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vector>
#include <memory>

#include <villas/mapping_list.hpp>
#include <villas/pool.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;
class Node;
class Path;

class PathSource {
	friend Path;

public:
	using Ptr = std::shared_ptr<PathSource>;

protected:
	Node *node;
	Path *path;

	bool masked;

	struct Pool pool;

	MappingList mappings;				// List of mappings (struct MappingEntry).

public:
	PathSource(Path *p, Node *n);
	virtual
	~PathSource();

	void check();

	int read(int i);

	virtual
	void writeToSecondaries(struct Sample *smps[], unsigned cnt)
	{ }

	Node * getNode() const
	{
		return node;
	}

	Path * getPath() const
	{
		return path;
	}
};

using PathSourceList = std::vector<PathSource::Ptr>;

class SecondaryPathSource : public PathSource {

protected:
	PathSource::Ptr master;

public:
	using Ptr = std::shared_ptr<SecondaryPathSource>;

	SecondaryPathSource(Path *p, Node *n, NodeList &nodes, PathSource::Ptr master);
};

using SecondaryPathSourceList = std::vector<SecondaryPathSource::Ptr>;

class MasterPathSource : public PathSource {

protected:
	SecondaryPathSourceList secondaries;	// List of secondary path sources (PathSource).

public:
	MasterPathSource(Path *p, Node *n);

	void addSecondary(SecondaryPathSource::Ptr ps)
	{
		secondaries.push_back(ps);
	}

	SecondaryPathSourceList getSecondaries()
	{
		return secondaries;
	}

	virtual
	void writeToSecondaries(struct Sample *smps[], unsigned cnt);
};


} // namespace node
} // namespace villas
