/** API session.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <stdbool.h>
#include <jansson.h>

#include "common.h"
#include "web/buffer.h"

enum api_version {
	API_VERSION_UNKOWN	= 0,
	API_VERSION_1		= 1
};

enum api_mode {
	API_MODE_WS,	/**< This API session was established over a WebSocket connection. */
	API_MODE_HTTP	/**< This API session was established via a HTTP REST request. */
};

/** A connection via HTTP REST or WebSockets to issue API actions. */
struct api_session {
	enum api_mode mode;
	enum api_version version;
	
	int runs;

	struct {
		struct web_buffer body;		/**< HTTP body / WS payload */
	} request;

	struct {
		struct web_buffer body;		/**< HTTP body / WS payload */
		struct web_buffer headers;	/**< HTTP headers */
	} response;
	
	bool completed;				/**< Did we receive the complete body yet? */
	
	enum state state;
	
	struct api *api;
};

int api_session_init(struct api_session *s, struct api *a, enum api_mode m);

int api_session_destroy(struct api_session *s);

int api_session_run_command(struct api_session *s, json_t *req, json_t **resp);
