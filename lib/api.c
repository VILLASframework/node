/** REST-API-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <libwebsockets.h>

#include "plugin.h"
#include "api.h"
#include "log.h"
#include "config.h"

static int api_parse_request(struct api_buffer *b, json_t **req)
{
	json_error_t e;
	
	if (b->len <= 0)
		return -1;

	*req = json_loadb(b->buf, b->len, JSON_DISABLE_EOF_CHECK, &e);
	if (!*req)
		return -1;

	if (e.position < b->len) {
		void *dst = (void *) b->buf;
		void *src = (void *) (b->buf + e.position);
		
		memmove(dst, src, b->len - e.position);
		
		b->len -= e.position;
	}
	else
		b->len = 0;

	return 1;
}

static int api_unparse_response(struct api_buffer *b, json_t *res)
{
	size_t len;

retry:	len = json_dumpb(res, b->buf + b->len, b->size - b->len, 0);
	if (len > b->size - b->len) {
		b->buf = realloc(b->buf, b->len + len);
		b->size += len;
		goto retry;
	}
	else
		b->len += len;
	
	return 0;
}

int api_session_run_command(struct api_session *s, json_t *json_in, json_t **json_out)
{
	int ret;
	const char *rstr;
	char *id;
	struct plugin *p;
	
	json_t *json_args = NULL, *json_resp;
	
	ret = json_unpack(json_in, "{ s: s, s: s, s?: o }",
		"request", &rstr,
		"id", &id,
		"args", &json_args);
	if (ret) {
		*json_out = json_pack("{ s: s, s: s, s: i, s: s }",
				"command", rstr,
				"error", "invalid request",
				"code", -1,
				"id", id);
		goto out;
	}
	
	p = plugin_lookup(PLUGIN_TYPE_API, rstr);
	if (!p) {
		*json_out = json_pack("{ s: s, s: s, s: d, s: s, s: s }",
				"command", rstr,
				"error", "command not found",
				"code", -2,
				"command", rstr,
				"id", id);
		goto out;
	}

	debug(LOG_API, "Running API request: %s", p->name);

	ret = p->api.cb(&p->api, json_args, &json_resp, s);
	if (ret)
		*json_out = json_pack("{ s: s, s: s, s: s }",
				"command", rstr,
				"error", "command failed",
				"code", ret,
				"id", id);
	else
		*json_out = json_pack("{ s: s, s: o, s: s }",
				"command", rstr,
				"response", json_resp,
				"id", id);

out:	debug(LOG_API, "API request completed with code: %d", ret);

	return 0;
}

int api_ws_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	//struct api_session *s = (struct api_session *) user;

	switch (reason) {
		default:
			break;
	}
	
	return 0;
}

int api_http_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;

	struct api_session *s = (struct api_session *) user;

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED: {
			struct web *w = (struct web *) lws_context_user(lws_get_context(wsi));
			
			if (w->api == NULL)
				return -1; /** @todo return error message */
			
			api_session_init(s, w->api, API_MODE_WS);
			break;
		}
		
		case LWS_CALLBACK_HTTP: {
			struct web *w = (struct web *) lws_context_user(lws_get_context(wsi));

			char *uri = (char *) in;

			/* Parse request URI */
			ret = sscanf(uri, "/api/v%d", (int *) &s->version);
			if (ret != 1)
				return -1;
						
			debug(LOG_API, "New REST API session initiated: version = %d", s->version);
			
			api_session_init(s, w->api, API_MODE_HTTP);

			/* Prepare HTTP response header */
			const char headers[] =	"HTTP/1.1 200 OK\r\n"
						"Content-type: application/json\r\n"
						"User-agent: " USER_AGENT "\r\n"
						"\r\n";
			
			api_buffer_append(&s->response.headers, headers, sizeof(headers)-1);

			/* book us a LWS_CALLBACK_HTTP_WRITEABLE callback */
			lws_callback_on_writable(wsi);

			break;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
		case LWS_CALLBACK_HTTP_BODY: {			
			api_buffer_append(&s->request.body, in, len);
			
			json_t *req, *resp;
			while (api_parse_request(&s->request.body, &req) == 1) {
				api_session_run_command(s, req, &resp);
				api_unparse_response(&s->response.body, resp);

				lws_callback_on_writable(wsi);
			}
			
			break;
		}
			
		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
			s->completed = true;
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
		case LWS_CALLBACK_HTTP_WRITEABLE:
			/* We send headers only in HTTP mode */
			if (s->mode == API_MODE_HTTP)
				api_buffer_send(&s->response.headers, wsi, LWS_WRITE_HTTP_HEADERS);

			api_buffer_send(&s->response.body, wsi, LWS_WRITE_HTTP);
			
			if (s->completed && s->response.body.len == 0)
				return -1;

			break;

		default:
			return 0;
	}

	return 0;
}

int api_buffer_send(struct api_buffer *b, struct lws *w, enum lws_write_protocol prot)
{
	int sent;
	
	if (b->len <= 0)
		return 0;

	sent = lws_write(w, (unsigned char *) b->buf, b->len, prot);
	if (sent > 0) {
		memmove(b->buf, b->buf + sent, sent);
		b->len -= sent;
	}
	
	return sent;
}

int api_buffer_append(struct api_buffer *b, const char *in, size_t len)
{
	b->buf = realloc(b->buf, b->len + len);
	if (!b->buf)
		return -1;
	
	memcpy(b->buf + b->len, in, len);

	b->len += len;
	
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

int api_session_init(struct api_session *s, struct api *a, enum api_mode m)
{
	s->mode = m;
	s->api = a;
	
	s->completed = false;
	
	s->request.body = 
	s->response.body =
	s->response.headers = (struct api_buffer) {
		.buf = NULL,
		.size = 0,
		.len = 0
	};
	
	return 0;
}

int api_session_destroy(struct api_session *s)
{
	return 0;
}
