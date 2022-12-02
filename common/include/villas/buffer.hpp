/** A simple buffer for encoding streamed JSON messages.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <vector>

#include <cstdlib>

#include <jansson.h>

namespace villas {

class Buffer : public std::vector<char> {

protected:
	static int callback(const char *data, size_t len, void *ctx);

public:
	Buffer(const char *buf, size_type len) :
		std::vector<char>(buf, buf+len)
	{ }

	Buffer(size_type count = 0) :
		std::vector<char>(count, 0)
	{ }

	// Encode JSON document /p j and append it to the buffer
	int encode(json_t *j, int flags = 0);

	// Decode JSON document from the beginning of the buffer
	json_t * decode();

	void append(const char *data, size_t len)
	{
		insert(end(), data, data + len);
	}
};

} // namespace villas
