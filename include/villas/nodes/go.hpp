/** Node-type implemeted in Go language
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#pragma once

#include <villas/node.hpp>

namespace villas {
namespace node {

/* Forward declarations */
struct Sample;
class Format;
class GoNodeFactory;

class GoNode : public Node {

	friend GoNodeFactory;

protected:
	uintptr_t node; // runtime/cgo.Handle

	Format *formatter;

	virtual
	int _read(struct Sample * smps[], unsigned cnt);

	virtual
	int _write(struct Sample * smps[], unsigned cnt);

public:
	GoNode(uintptr_t n);

	virtual
	~GoNode();

	virtual
	int parse(json_t *json, const uuid_t sn_uuid);

	virtual
	std::vector<int> getPollFDs();

	virtual
	std::vector<int> getNetemFDs();

	virtual
	std::string getDetails();

	virtual
	int prepare();

	virtual
	int check();

	virtual
	int start();

	virtual
	int stop();

	virtual
	int pause();

	virtual
	int resume();

	virtual
	int restart();

	virtual
	int reverse();
};

class GoNodeFactory : public NodeFactory {

protected:
	std::string name;
	std::string description;
	int flags;

public:
	GoNodeFactory(char *n, char *d, int f = 0) :
		NodeFactory(),
		name(n),
		description(d),
		flags(f)
	{ }

	virtual
	Node * make();

	virtual
	std::string getName() const
	{
		return name;
	}

	virtual
	std::string getDescription() const
	{
		return description;
	}

	virtual
	int getFlags() const
	{
		return flags;
	}
};

class GoPluginRegistry : public plugin::SubRegistry {

public:
	GoPluginRegistry();

	plugin::List<> lookup();
};

} /* namespace node */
} /* namespace villas */
