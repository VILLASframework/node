/** A simple buffer for encoding streamed JSON messages.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <vector>

#include <cstdlib>

#include <jansson.h>

namespace villas {

class JsonBuffer : public std::vector<char>
{

protected:
	static int callback(const char *data, size_t len, void *ctx);

public:
	/** Encode JSON document /p j and append it to the buffer */
	int encode(json_t *j, int flags = 0);

	/** Decode JSON document from the beginning of the buffer */
	json_t * decode();

	void append(const char *data, size_t len)
	{
		insert(end(), data, data + len);
	}
};

} /* namespace villas */
