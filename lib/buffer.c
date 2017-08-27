/** A simple growing buffer.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <string.h>

#include "buffer.h"
#include "common.h"

int buffer_init(struct buffer *b, size_t size)
{
	b->len = 0;
	b->size = size;
	b->buf = malloc(size);
	if (!b->buf)
		return -1;
	
	b->state = STATE_INITIALIZED;
	
	return 0;
}

int buffer_destroy(struct buffer *b)
{
	if (b->buf)
		free(b->buf);
	
	b->state = STATE_DESTROYED;
	
	return 0;
}

void buffer_clear(struct buffer *b)
{
	b->len = 0;
}

int buffer_append(struct buffer *b, const char *data, size_t len)
{
	if (b->len + len > b->size) {
		b->size = b->len + len;
		b->buf = realloc(b->buf, b->size);
		if (!b->buf)
			return -1;
	}

	memcpy(b->buf + b->len, data, len);

	b->len += len;
	
	return 0;
}

int buffer_parse_json(struct buffer *b, json_t **j)
{
	*j = json_loadb(b->buf, b->len, 0, NULL);
	if (!*j)
		return -1;
	
	return 0;
}

int buffer_append_json(struct buffer *b, json_t *j)
{
	size_t len;

retry:	len = json_dumpb(j, b->buf + b->len, b->size - b->len, 0);
	if (b->size < b->len + len) {
		b->buf = realloc(b->buf, b->len + len);
		if (!b->buf)
			return -1;

		b->size = b->len + len;
		goto retry;
	}

	b->len += len;

	return 0;
}
