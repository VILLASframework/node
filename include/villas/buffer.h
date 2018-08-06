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

#pragma once

#include <stdlib.h>

#include <jansson.h>

#include <villas/common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct buffer {
	enum state state;

	char *buf;
	size_t len;
	size_t size;
};

int buffer_init(struct buffer *b, size_t size);

int buffer_destroy(struct buffer *b);

void buffer_clear(struct buffer *b);

int buffer_append(struct buffer *b, const char *data, size_t len);

int buffer_parse_json(struct buffer *b, json_t **j);

int buffer_append_json(struct buffer *b, json_t *j);

#ifdef __cplusplus
}
#endif
