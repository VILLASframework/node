/** Node list
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 **********************************************************************************/


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

} // namespace node
} // namespace villas
