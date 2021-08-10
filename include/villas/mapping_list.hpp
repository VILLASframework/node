#pragma once

/** Sample value remapping for path source muxing.
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
 *********************************************************************************/

#include <list>

#include <villas/mapping.hpp>
#include <villas/node_list.hpp>

namespace villas {
namespace node {

class MappingList : public std::list<MappingEntry::Ptr> {

public:
	int parse(json_t *json);

	int prepare(NodeList &nodes);

	int remap(struct Sample *remapped, const struct Sample *original) const;

	int update(const MappingEntry::Ptr me, struct Sample *remapped, const struct Sample *original);
};

} /* namespace node */
} /* namespace villas */
