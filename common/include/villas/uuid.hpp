/* UUID helpers.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

#include <jansson.h>
#include <uuid/uuid.h>

typedef char uuid_string_t[37];

namespace villas {
namespace uuid {

// Convert a UUID to C++ string
std::string toString(const uuid_t in);

// Generate an UUID by MD5 hashing the provided string
int generateFromString(uuid_t out, const std::string &data, const std::string &ns = "");

// Generate an UUID by MD5 hashing the provided string
int generateFromString(uuid_t out, const std::string &data, const uuid_t ns);

// Generate an UUID by MD5 hashing the serialized representation of the provided JSON object
void generateFromJson(uuid_t out, json_t *json, const std::string &ns = "");

// Generate an UUID by MD5 hashing the serialized representation of the provided JSON object
int generateFromJson(uuid_t out, json_t *json, const uuid_t ns);

} // namespace uuid
} // namespace villas
