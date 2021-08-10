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
#include <villas/node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;

Node * NodeList::lookup(const uuid_t &uuid)
{
	for (auto *n : *this) {
		if (!uuid_compare(uuid, n->getUuid()))
			return n;
	}

	return nullptr;
}

Node * NodeList::lookup(const std::string &name)
{
	for (auto *n : *this) {
		if (name == n->getNameShort())
			return n;
	}

	return nullptr;
}

int NodeList::parse(json_t *json, NodeList &all)
{
	Node *node;
	const char *str;

	size_t index;
	json_t *elm;

	auto logger = logging.get("node");

	switch (json_typeof(json)) {
		case JSON_STRING:
			str = json_string_value(json);
			node = all.lookup(str);
			if (!node)
				goto invalid2;

			push_back(node);
			break;

		case JSON_ARRAY:
			json_array_foreach(json, index, elm) {
				if (!json_is_string(elm))
					goto invalid;

				str = json_string_value(elm);
				node = all.lookup(str);
				if (!node)
					goto invalid;

				push_back(node);
			}
			break;

		default:
			goto invalid;
	}

	return 0;

invalid:
	throw RuntimeError("The node list must be an a single or an array of strings referring to the keys of the 'nodes' section");

	return -1;

invalid2:
	std::stringstream allss;
	for (auto *n : all)
		allss << " " << n->getNameShort();

	throw RuntimeError("Unknown node {}. Choose of one of:{}", str, allss.str());

	return 0;
}

json_t * NodeList::toJson() const
{
	json_t *json_nodes = json_array();

	for (const auto *n : *this)
		json_array_append_new(json_nodes, n->toJson());

	return json_nodes;
}
