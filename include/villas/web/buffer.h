/** WebSocket buffer.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <stddef.h>

#include <libwebsockets.h>

#include "common.h"

struct web_buffer {
	char *buffer;	/**< A pointer to the buffer. Usually resized via realloc() */
	size_t size;	/**< The allocated size of the buffer. */
	size_t len;	/**< The used length of the buffer. */
	size_t prefix;	/**< The used length of the buffer. */

	enum lws_write_protocol protocol;
	
	enum state state;
};

/** Initialize a libwebsockets buffer. */
int web_buffer_init(struct web_buffer *b, enum lws_write_protocol prot);

/** Destroy a libwebsockets buffer. */
int web_buffer_destroy(struct web_buffer *b);

/** Flush the buffers contents to lws_write() */
int web_buffer_write(struct web_buffer *b, struct lws *w);

/** Copy \p len bytes from the beginning of the buffer and copy them to \p out.
 *
 * @param out The destination buffer. If NULL, we just remove \p len bytes from the buffer.
 */
int web_buffer_read(struct web_buffer *b, char *out, size_t len);

/** Parse JSON from the beginning of the buffer.
 *
 * @retval -1 The buffer is empty.
 * @retval -2 The buffer contains malformed JSON.
 */
int web_buffer_read_json(struct web_buffer *b, json_t **req);

/** Append \p len bytes of \p in at the end of the buffer.
 *
 * The buffer is automatically resized.
 */
int web_buffer_append(struct web_buffer *b, const char *in, size_t len);

/** Append the serialized represetnation of the JSON object \p res at the end of the buffer. */
int web_buffer_append_json(struct web_buffer *b, json_t *res);
