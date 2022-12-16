/** Path list
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 **********************************************************************************/


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
