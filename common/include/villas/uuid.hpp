/** UUID helpers.
 *
 * @file
 * @author Steffen Vogel <github@daniel-krebs.net>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#pragma once

#include <string>

#include <jansson.h>
#include <uuid/uuid.h>

namespace villas {
namespace uuid {

/** Generate an UUID by MD5 hashing the provided string */
int generateFromString(uuid_t out, const std::string &data, const std::string &ns = "");

/** Generate an UUID by MD5 hashing the provided string */
int generateFromString(uuid_t out, const std::string &data, const uuid_t ns);

/** Generate an UUID by MD5 hashing the serialized representation of the provided JSON object */
void generateFromJson(uuid_t out, json_t *json, const std::string &ns = "");

/** Generate an UUID by MD5 hashing the serialized representation of the provided JSON object */
int generateFromJson(uuid_t out, json_t *json, const uuid_t ns);

} /* namespace uuid */
} /* namespace villas */
