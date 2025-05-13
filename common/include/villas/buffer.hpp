/* A simple buffer for encoding streamed JSON messages.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdlib>
#include <vector>

#include <jansson.h>

namespace villas {

class Buffer : public std::vector<char> {

protected:
  static int callback(const char *data, size_t len, void *ctx);

public:
  Buffer(const char *buf, size_type len) : std::vector<char>(buf, buf + len) {}

  Buffer(size_type count = 0) : std::vector<char>(count, 0) {}

  // Encode JSON document /p j and append it to the buffer
  int encode(json_t *j, int flags = 0);

  // Decode JSON document from the beginning of the buffer
  json_t *decode();

  void append(const char *data, size_t len) { insert(end(), data, data + len); }
};

} // namespace villas
