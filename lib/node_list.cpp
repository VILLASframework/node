/** Node list
 *
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

#include <villas/node_list.hpp>
#include <villas/node.h>

using namespace villas::node;

struct vnode * NodeList::lookup(const uuid_t &uuid)
{
	for (auto *n : *this) {
		if (!uuid_compare(uuid, n->uuid))
			return n;
	}

	return nullptr;
}

struct vnode * NodeList::lookup(const std::string &name)
{
	for (auto *n : *this) {
		if (name == n->name)
			return n;
	}

	return nullptr;
}
