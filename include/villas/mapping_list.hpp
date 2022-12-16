#pragma once

/** Sample value remapping for path source muxing.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
