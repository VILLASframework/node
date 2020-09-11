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
	buf(sz, 0)
{ }

void Buffer::clear()
{
	buf.clear();
}

int Buffer::append(const char *data, size_t l)
{
	buf.insert(buf.end(), data, data+l);

	return 0;
}

int Buffer::parseJson(json_t **j)
{
	*j = json_loadb(buf.data(), buf.size(), 0, nullptr);
	if (!*j)
		return -1;

	return 0;
}

int Buffer::appendJson(json_t *j, int flags)
{
	size_t l;

retry:	l = json_dumpb(j, buf.data() + buf.size(), buf.capacity() - buf.size(), flags);
	if (buf.capacity() < buf.size() + l) {
		buf.reserve(buf.size() + l);
		goto retry;
	}

	return 0;
}
