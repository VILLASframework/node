/* Path list.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/path.hpp>
#include <villas/path_list.hpp>

using namespace villas::node;

Path *PathList::lookup(const uuid_t &uuid) const {
  for (auto *p : *this) {
    if (!uuid_compare(uuid, p->uuid))
      return p;
  }

  return nullptr;
}

json_t *PathList::toJson() const {
  json_t *json_paths = json_array();

  for (auto *p : *this)
    json_array_append_new(json_paths, p->toJson());

  return json_paths;
}
