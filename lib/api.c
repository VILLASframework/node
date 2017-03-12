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

static int parse_request(struct api_buffer *b, json_t **req)
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

	return 1;
}

#if JANSSON_VERSION_HEX < 0x020A00
size_t json_dumpb(const json_t *json, char *buffer, size_t size, size_t flags)
{
	char *str;
	size_t len;

	str = json_dumps(json, flags);
	if (!str)
		return 0;
	
	len = strlen(str) - 1; // not \0 terminated
	if (buffer && len <= size)
		memcpy(buffer, str, len);

	//free(str);

	return len;
}
#endif

static int unparse_response(struct api_buffer *b, json_t *res)
{
	size_t len;

retry:	len = json_dumpb(res, b->buf + b->len, b->size - b->len, 0);
	if (len > b->size - b->len) {
		b->buf = realloc(b->buf, b->len + len);
		b->len += len;
		goto retry;
	}
	
	return 0;
}

int api_session_run_command(struct api_session *s, json_t *req, json_t **resp)
{
	int ret;
	const char *rstr;
	struct plugin *p;
	
	json_t *args;
	
	ret = json_unpack(req, "{ s: s, s: o }",
		"command", &rstr,
		"arguments", &args);
	if (ret)
		*resp = json_pack("{ s: s, s: d }",
				"error", "invalid request",
				"code", -1);
	
	p = plugin_lookup(PLUGIN_TYPE_API, rstr);
	if (!p)
		*resp = json_pack("{ s: s, s: d, s: s }",
				"error", "command not found",
				"code", -2,
				"command", rstr);
				
	debug(LOG_API, "Running API request: %s with arguments: %s", p->name, json_dumps(args, 0));

	ret = p->api.cb(&p->api, args, resp, s);
	if (ret)
		*resp = json_pack("{ s: s, s: d }",
				"error", "command failed",
				"code", ret);

	debug(LOG_API, "API request completed with code: %d and output: %s", ret, json_dumps(*resp, 0));

	return 0;
}

int api_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;

	struct api_session *s = (struct api_session *) user;

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED: {
			struct web *w = (struct web *) lws_context_user(lws_get_context(wsi));
			
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
						"User-agent: " USER_AGENT
						"\r\n";
			
			api_buffer_append(&s->response.headers, headers, sizeof(headers));

			/* book us a LWS_CALLBACK_HTTP_WRITEABLE callback */
			lws_callback_on_writable(wsi);

			break;
		}

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
		case LWS_CALLBACK_HTTP_BODY: {			
			api_buffer_append(&s->request.body, in, len);
			
			json_t *req, *resp;
			while (parse_request(&s->request.body, &req) == 1) {

				api_session_run_command(s, req, &resp);

				unparse_response(&s->response.body, resp);
				
				debug(LOG_WEB, "Sending response: %s len=%zu", s->response.body.buf, s->response.body.len);

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
				api_buffer_send(&s->response.headers, wsi);

			api_buffer_send(&s->response.body, wsi);
			
			if (s->completed && s->response.body.len == 0)
				return -1;

			break;

		default:
			return 0;
	}

	return 0;
}

int api_buffer_send(struct api_buffer *b, struct lws *w)
{
	int sent;
	
	if (b->len <= 0)
		return 0;
	
	sent = lws_write(w, (unsigned char *) b->buf, b->len, LWS_WRITE_HTTP_HEADERS);
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

int api_init(struct api *a, struct cfg *cfg)
{
	info("Initialize API sub-system");

	list_init(&a->sessions);

	a->cfg = cfg;
	a->state = STATE_INITIALIZED;

	return 0;
}

int api_destroy(struct api *a)
{
	if (a->state == STATE_STARTED)
		return -1;

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
	s->response.headers = (struct api_buffer) { 0 };
	
	return 0;
}

int api_session_destroy(struct api_session *s)
{
	return 0;
}
