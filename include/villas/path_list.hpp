/** Node list
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
 */

#pragma once

#include <uuid/uuid.h>
#include <jansson.h>

#include <list>
#include <string>

namespace villas {
namespace node {

/* Forward declarations */
class Path;

class PathList : public std::list<Path *> {

public:
	/** Lookup a path from the list based on its UUID */
	Path * lookup(const uuid_t &uuid) const;

	json_t * toJson() const;
};

} /* namespace node */
} /* namespace villas */
