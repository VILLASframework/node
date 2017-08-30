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
#include "memory.h"
#include "compat.h"

/* Forward declarations */
static void * api_worker(void *ctx);

int api_ws_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret, pulled, pushed;
	json_t *req, *resp;

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
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);

			ret = sscanf(uri, "/v%d", (int *) &s->version);
			if (ret != 1)
				return -1;

			ret = api_session_init(s, API_MODE_WS);
			if (ret)
				return -1;
			
			s->wsi = wsi;
			s->api = w->api;
			
			list_push(&s->api->sessions, s);

			debug(LOG_API, "Initiated API session: %s", api_session_name(s));

			break;

		case LWS_CALLBACK_CLOSED:
			ret = api_session_destroy(s);
			if (ret)
				return -1;
			
			list_remove(&w->api->sessions, s);

			debug(LOG_API, "Closing API session: %s", api_session_name(s));

			break;

		case LWS_CALLBACK_RECEIVE:
			if (lws_is_first_fragment(wsi))
				buffer_clear(&s->request.buffer);
		
			buffer_append(&s->request.buffer, in, len);
			
			if (lws_is_final_fragment(wsi)) {
				ret = buffer_parse_json(&s->request.buffer, &req);
				if (ret)
					break;
				
				pushed = queue_push(&s->request.queue, req);
				if (pushed != 1)
					warn("Queue overun in API session");

				pushed = queue_signalled_push(&w->api->pending, s);
				if (pushed != 1)
					warn("Queue overrun in API");
			}

			break;
		
		case LWS_CALLBACK_SERVER_WRITEABLE:
			pulled = queue_pull(&s->response.queue, (void **) &resp);
			if (pulled < 1)
				break;
			
			char pad[LWS_PRE];
			
			buffer_clear(&s->response.buffer);
			buffer_append(&s->response.buffer, pad, sizeof(pad));
			buffer_append_json(&s->response.buffer, resp);

			lws_write(wsi, (unsigned char *) s->response.buffer.buf + LWS_PRE, s->response.buffer.len - LWS_PRE, LWS_WRITE_TEXT);
			break;

		default:
			return 0;
	}

	return 0;
}

int api_http_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret, pulled, pushed;
	json_t *resp, *req;

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

			ret = api_session_init(s, API_MODE_HTTP);
			if (ret)
				return -1;

			s->wsi = wsi;
			s->api = w->api;
			
			list_push(&s->api->sessions, s);

			debug(LOG_API, "Initiated API session: %s", api_session_name(s));

			break;

		case LWS_CALLBACK_CLOSED_HTTP:
			if (!s)
				return -1;

			ret = api_session_destroy(s);
			if (ret)
				return -1;
			
			if (w->api->sessions.state == STATE_INITIALIZED)
				list_remove(&w->api->sessions, s);
			
			break;

		case LWS_CALLBACK_HTTP_BODY:
			buffer_append(&s->request.buffer, in, len);

			break;

		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
			ret = buffer_parse_json(&s->request.buffer, &req);
			if (ret)
				break;

			buffer_clear(&s->request.buffer);
			
			pushed = queue_push(&s->request.queue, req);
			if (pushed != 1)
				warn("Queue overrun for API session: %s", api_session_name(s));

			pushed = queue_signalled_push(&w->api->pending, s);
			if (pushed != 1)
				warn("Queue overrun for API");

			break;

		case LWS_CALLBACK_HTTP_WRITEABLE:
			pulled = queue_pull(&s->response.queue, (void **) &resp);
			if (pulled) {
				const char headers[] =	"HTTP/1.1 200 OK\r\n"
					"Content-type: application/json\r\n"
					"User-agent: " USER_AGENT "\r\n"
					"Access-Control-Allow-Origin: *\r\n"
					"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
					"Access-Control-Allow-Headers: Content-Type\r\n"
					"Access-Control-Max-Age: 86400\r\n"
					"\r\n";
				
				buffer_clear(&s->response.buffer);
				buffer_append_json(&s->response.buffer, resp);
			
				lws_write(wsi, (unsigned char *) headers, strlen(headers), LWS_WRITE_HTTP_HEADERS);
				lws_write(wsi, (unsigned char *) s->response.buffer.buf, s->response.buffer.len, LWS_WRITE_HTTP);
				
				debug(LOG_API, "Closing API session: %s", api_session_name(s));

				return -1; /* Close connection */
			}

			break;

		default:
			return 0;
	}

	return 0;
}

int api_init(struct api *a, struct super_node *sn)
{
	int ret;

	info("Initialize API sub-system");

	ret = list_init(&a->sessions);
	if (ret)
		return ret;
	
	ret = queue_signalled_init(&a->pending, 1024, &memtype_heap, 0);
	if (ret)
		return ret;

	a->super_node = sn;
	a->state = STATE_INITIALIZED;

	return 0;
}

int api_destroy(struct api *a)
{
	int ret;

	assert(a->state != STATE_STARTED);
	
	ret = queue_signalled_destroy(&a->pending);
	if (ret)
		return ret;

	a->state = STATE_DESTROYED;

	return 0;
}

int api_start(struct api *a)
{
	int ret;

	info("Starting API sub-system");
	
	ret = pthread_create(&a->thread, NULL, api_worker, a);
	if (ret)
		error("Failed to start API worker thread");

	a->state = STATE_STARTED;

	return 0;
}

int api_stop(struct api *a)
{
	int ret;

	info("Stopping API sub-system");
	
	for (int i = 0; i < 10 && list_length(&a->sessions) > 0; i++) { INDENT
		info("Wait for API requests to complete");
		usleep(1 * 1e6);
	}
	
	if (a->state != STATE_STARTED)
		return 0;

	ret = list_destroy(&a->sessions, (dtor_cb_t) api_session_destroy, false);
	if (ret)
		return ret;
	
	ret = pthread_cancel(a->thread);
	if (ret)
		serror("Failed to cancel API worker thread");

	ret = pthread_join(a->thread, NULL);
	if (ret)
		serror("Failed to join API worker thread");

	a->state = STATE_STOPPED;

	return 0;
}

static void * api_worker(void *ctx)
{
	int pulled;

	struct api *a = ctx;
	struct api_session *s;
	
	json_t *req, *resp;

	for (;;) {
		pulled = queue_signalled_pull(&a->pending, (void **) &s);
		if (pulled != 1)
			continue;

		queue_pull(&s->request.queue, (void **) &req);

		api_session_run_command(s, req, &resp);
		
		queue_push(&s->response.queue, resp);
		
		lws_callback_on_writable(s->wsi);
	}
	
	return NULL;
}
