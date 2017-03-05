/** REST-API-releated functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include "list.h"

#include <jansson.h>

/* Forward declarations */
enum lws_callback_reasons;
struct lws;
struct cfg;

struct api;
struct api_ressource;
struct api_buffer;
struct api_session;

/** Callback type of command function
 *
 * @param[inout] c Command handle
 * @param[in] args JSON command arguments.
 * @param[out] resp JSON command response.
 * @param[in] i Execution context.
 */
typedef int (*api_cb_t)(struct api_ressource *c, json_t *args, json_t **resp, struct api_session *s);

enum api_version {
	API_VERSION_UNKOWN	= 0,
	API_VERSION_1		= 1
};

enum api_mode {
	API_MODE_WS,	/**< This API session was established over a WebSocket connection. */
	API_MODE_HTTP	/**< This API session was established via a HTTP REST request. */
};

struct api {
	struct list sessions;	/**< List of currently active connections */
	
	struct cfg *cfg;
};

struct api_buffer {
	char *buf;	/**< A pointer to the buffer. Usually resized via realloc() */
	size_t size;	/**< The allocated size of the buffer. */
	size_t len;	/**< The used length of the buffer. */
};

/** A connection via HTTP REST or WebSockets to issue API actions. */
struct api_session {
	enum api_mode mode;
	enum api_version version;
	
	int runs;

	struct {
		struct api_buffer body;		/**< HTTP body / WS payload */
	} request;

	struct {
		struct api_buffer body;		/**< HTTP body / WS payload */
		struct api_buffer headers;	/**< HTTP headers */
	} response;
	
	bool completed;				/**< Did we receive the complete body yet? */
	
	struct api *api;
};

/** Command descriptor
 *
 * Every command is described by a descriptor.
 */
struct api_ressource {
	char *name;
	char *description;
	api_cb_t cb;
};

/** Initalize the API.
 *
 * Save references to list of paths / nodes for command execution.
 */
int api_init(struct api *a, struct cfg *cfg);

int api_destroy(struct api *a);

int api_deinit(struct api *a);

int api_session_init(struct api_session *s, struct api *a, enum api_mode m);

int api_session_destroy(struct api_session *s);

int api_session_run_command(struct api_session *s, json_t *req, json_t **resp);

/** Libwebsockets callback for "api" endpoint */
int api_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);