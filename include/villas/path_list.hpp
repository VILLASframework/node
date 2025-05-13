/* Node list.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <list>
#include <string>

#include <jansson.h>
#include <uuid/uuid.h>

namespace villas {
namespace node {

// Forward declarations
class Path;

class PathList : public std::list<Path *> {

public:
  // Lookup a path from the list based on its UUID
  Path *lookup(const uuid_t &uuid) const;

  json_t *toJson() const;
};

} // namespace node
} // namespace villas
