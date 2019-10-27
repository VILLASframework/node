/** A simple buffer for encoding streamed JSON messages.
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

#include <villas/compat.hpp>
#include <villas/json_buffer.hpp>

using namespace villas;

json_t * JsonBuffer::decode()
{
	json_t *j;
	json_error_t err;

	j = json_loadb(data(), size(), JSON_DISABLE_EOF_CHECK, &err);
	if (!j)
		return nullptr;

	/* Remove decoded JSON document from beginning */
	erase(begin(), begin() + err.position);

	return j;
}

int JsonBuffer::encode(json_t *j)
{
	return json_dump_callback(j, callback, this, 0);
}

int JsonBuffer::callback(const char *data, size_t len, void *ctx)
{
	JsonBuffer *b = static_cast<JsonBuffer *>(ctx);

	/* Append junk of JSON to buffer */
	b->insert(b->end(), &data[0], &data[len]);

	return 0;
}
