/** Sample value remapping for path source muxing.
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

#include <villas/mapping_list.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;

int MappingList::parse(json_t *json)
{
	int ret;

	size_t i;
	json_t *json_entry;
	json_t *json_mapping;

	if (json_is_string(json)) {
		json_mapping = json_array();
		json_array_append(json_mapping, json);
	}
	else if (json_is_array(json))
		json_mapping = json_incref(json);
	else
		return -1;

	json_array_foreach(json_mapping, i, json_entry) {
		auto me = std::make_shared<MappingEntry>();
		if (!me)
			throw MemoryAllocationError();

		ret = me->parse(json_entry);
		if (ret)
			goto out;

		push_back(me);
	}

	ret = 0;

out:	json_decref(json_mapping);

	return ret;
}

int MappingList::remap(struct Sample *remapped, const struct Sample *original) const
{
	int ret;

	for (auto me : *this) {
		ret = me->update(remapped, original);
		if (ret)
			return ret;
	}

	return 0;
}

int MappingList::prepare(NodeList &nodes)
{
	int ret;

	unsigned off = 0;
	for (auto me : *this) {
		ret = me->prepare(nodes);
		if (ret)
			return ret;

		me->offset = off;
		off += me->length;
	}

	return 0;
}
