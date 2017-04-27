/** REST-API-releated functions.
 *
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

#include <libwebsockets.h>
#include <string.h>

#include "api.h"
#include "log.h"
#include "web.h"
#include "config.h"
#include "assert.h"
#include "compat.h"

int api_ws_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;

	struct web *w = lws_context_user(lws_get_context(wsi));
	struct api_session *s = (struct api_session *) user;

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			if (w->api == NULL) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, (unsigned char *) "API disabled", strlen("API disabled"));
				return -1;
			}

			/* Parse request URI */
			char uri[64];
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
			
			ret = sscanf(uri, "/v%d", (int *) &s->version);
			if (ret != 1)
				return -1;
			
			ret = api_session_init(s, w->api, API_MODE_WS);
			if (ret)
				return -1;

			debug(LOG_API, "New API session initiated: version=%d, mode=websocket", s->version);
			break;

		case LWS_CALLBACK_CLOSED:
			ret = api_session_destroy(s);
			if (ret)
				return -1;
			
			debug(LOG_API, "Closed API session");
			
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			web_buffer_write(&s->response.body, wsi);

			if (s->completed && s->response.body.len == 0)
				return -1;

			break;
			
		case LWS_CALLBACK_RECEIVE:
			web_buffer_append(&s->request.body, in, len);
			
			json_t *req, *resp;
			while (web_buffer_read_json(&s->request.body, &req) >= 0) {
				api_session_run_command(s, req, &resp);
				
				web_buffer_append_json(&s->response.body, resp);
				lws_callback_on_writable(wsi);
			}
			break;

		default:
			return 0;
	}

	return 0;
}

int api_http_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;

	struct web *w = lws_context_user(lws_get_context(wsi));
	struct api_session *s = (struct api_session *) user;

	switch (reason) {
		case LWS_CALLBACK_HTTP:
			if (w->api == NULL) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, (unsigned char *) "API disabled", strlen("API disabled"));
				return -1;
			}

			/* Parse request URI */
			ret = sscanf(in, "/api/v%d", (int *) &s->version);
			if (ret != 1)
				return -1;

			ret = api_session_init(s, w->api, API_MODE_HTTP);
			if (ret)
				return -1;

			debug(LOG_API, "New API session initiated: version=%d, mode=http", s->version);

			/* Prepare HTTP response header */
			const char headers[] =	"HTTP/1.1 200 OK\r\n"
						"Content-type: application/json\r\n"
						"User-agent: " USER_AGENT "\r\n"
						"\r\n";

			web_buffer_append(&s->response.headers, headers, sizeof(headers)-1);
			lws_callback_on_writable(wsi);
			break;
			
		case LWS_CALLBACK_CLOSED_HTTP:
			ret = api_session_destroy(s);
			if (ret)
				return -1;
			break;

		case LWS_CALLBACK_HTTP_BODY:
			web_buffer_append(&s->request.body, in, len);
			
			json_t *req, *resp;
			while (web_buffer_read_json(&s->request.body, &req) == 1) {
				api_session_run_command(s, req, &resp);

				web_buffer_append_json(&s->response.body, resp);
				lws_callback_on_writable(wsi);
			}
			break;

		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
			s->completed = true;
			break;

		case LWS_CALLBACK_HTTP_WRITEABLE:
			web_buffer_write(&s->response.headers, wsi);
			web_buffer_write(&s->response.body,    wsi);

			if (s->completed && s->response.body.len == 0)
				return -1;
			break;

		default:
			return 0;
	}

	return 0;
}

int api_init(struct api *a, struct super_node *sn)
{
	info("Initialize API sub-system");

	list_init(&a->sessions);

	a->super_node = sn;
	a->state = STATE_INITIALIZED;

	return 0;
}

int api_destroy(struct api *a)
{
	assert(a->state != STATE_STARTED);

	a->state = STATE_DESTROYED;

	return 0;
}

int api_start(struct api *a)
{
	info("Starting API sub-system");

	a->state = STATE_STARTED;

	return 0;
}

int api_stop(struct api *a)
{
	info("Stopping API sub-system");

	list_destroy(&a->sessions, (dtor_cb_t) api_session_destroy, false);
	
	a->state = STATE_STOPPED;
	
	return 0;
}
