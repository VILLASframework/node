/** A simple growing buffer.
 *
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

#include <cstring>

#include <villas/compat.hpp>
#include <villas/buffer.hpp>
#include <villas/common.hpp>
#include <villas/exceptions.hpp>

using namespace villas;

Buffer::Buffer(size_t sz) :
	len(0),
	size(sz)
{
	buf = new char[size];
	if (!buf)
		throw RuntimeError("Failed to allocate memory");

	memset(buf, 0, size);
}

Buffer::~Buffer()
{
	delete[] buf;
}

void Buffer::clear()
{
	len = 0;
}

int Buffer::append(const char *data, size_t l)
{
	if (len + l > size) {
		size = len + l;
		buf = (char *) realloc(buf, size);
		if (!buf)
			return -1;
	}

	memcpy(buf + len, data, l);

	len += l;

	return 0;
}

int Buffer::parseJson(json_t **j)
{
	*j = json_loadb(buf, len, 0, nullptr);
	if (!*j)
		return -1;

	return 0;
}

int Buffer::appendJson(json_t *j)
{
	size_t l;

retry:	l = json_dumpb(j, buf + len, size - len, 0);
	if (size < len + l) {
		buf = (char *) realloc(buf, len + l);
		if (!buf)
			return -1;

		size = len + l;
		goto retry;
	}

	len += l;

	return 0;
}
