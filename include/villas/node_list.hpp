/** Node list
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
 */

#pragma once

#include <uuid/uuid.h>
#include <jansson.h>

#include <list>
#include <string>

namespace villas {
namespace node {

/* Forward declarations */
class Node;

class NodeList : public std::list<Node *> {

public:
	/** Lookup a node from the list based on its name */
	Node * lookup(const std::string &name);

	/** Lookup a node from the list based on its UUID */
	Node * lookup(const uuid_t &uuid);

	/** Parse an array or single node and checks if they exist in the "nodes" section.
	 *
	 * Examples:
	 *     out = [ "sintef", "scedu" ]
	 *     out = "acs"
	 *
	 * @param json A JSON array or string. See examples above.
	 * @param nodes The nodes will be added to this list.
	 * @param all This list contains all valid nodes.
	 */
	int parse(json_t *json, NodeList &all);

	json_t * toJson() const;
};

} /* namespace node */
} /* namespace villas */
