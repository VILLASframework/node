/* A simple buffer for encoding streamed JSON messages.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/compat.hpp>
#include <villas/buffer.hpp>

using namespace villas;

json_t * Buffer::decode()
{
	json_t *j;
	json_error_t err;

	j = json_loadb(data(), size(), JSON_DISABLE_EOF_CHECK, &err);
	if (!j)
		return nullptr;

	// Remove decoded JSON document from beginning
	erase(begin(), begin() + err.position);

	return j;
}

int Buffer::encode(json_t *j, int flags)
{
	return json_dump_callback(j, callback, this, flags);
}

int Buffer::callback(const char *data, size_t len, void *ctx)
{
	Buffer *b = static_cast<Buffer *>(ctx);

	// Append junk of JSON to buffer
	b->insert(b->end(), &data[0], &data[len]);

	return 0;
}
