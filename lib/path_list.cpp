/** Path list
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

#include <villas/path_list.hpp>
#include <villas/path.hpp>

using namespace villas::node;

Path * PathList::lookup(const uuid_t &uuid) const
{
	for (auto *p : *this) {
		if (!uuid_compare(uuid, p->uuid))
			return p;
	}

	return nullptr;
}

json_t * PathList::toJson() const
{
	json_t *json_paths = json_array();

	for (auto *p : *this)
		json_array_append_new(json_paths, p->toJson());

	return json_paths;
}
