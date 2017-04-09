/** REST-API-releated functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <libwebsockets.h>
#include <jansson.h>

#include "list.h"
#include "common.h"

/* Forward declarations */
struct lws;
struct super_node;

struct api;
struct api_ressource;
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
	
	enum state state;
	
	struct super_node *super_node;
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
	api_cb_t cb;
};

/** Initalize the API.
 *
 * Save references to list of paths / nodes for command execution.
 */
int api_init(struct api *a, struct super_node *sn);

int api_destroy(struct api *a);

int api_start(struct api *a);

int api_stop(struct api *a);

int api_session_init(struct api_session *s, struct api *a, enum api_mode m);

int api_session_destroy(struct api_session *s);

int api_session_run_command(struct api_session *s, json_t *req, json_t **resp);

/** Libwebsockets callback for "api" endpoint */
int api_ws_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

int api_http_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);