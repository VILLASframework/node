/** REST-API-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <libwebsockets.h>

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
		
		return 1;
	}

	return 0;
}

#if JANSSON_VERSION_HEX < 0x020A00
size_t json_dumpb(const json_t *json, char *buffer, size_t size, size_t flags)
{
	char *str = json_dumps(json, flags);
	size_t len = strlen(str) - 1; // not \0 terminated

	if (!buffer || len > size)
		return len;
	else
		memcpy(buffer, str, len);
	
	return len;
}
#endif

static int unparse_response(struct api_buffer *b, json_t *res)
{
	size_t len;

	retry:	len = json_dumpb(res, b->buf + b->len, b->size - b->len, 0);
	if (len > b->size - b->len) {
		b->buf = realloc(b->buf, b->len + len);
		goto retry;
	}
	
	return 0;
}

int api_session_run_command(struct api_session *s, json_t *req, json_t **resp)
{
	int ret;
	const char *rstr;
	struct api_ressource *res;
	
	json_t *args;
	
	ret = json_unpack(req, "{ s: s, s: o }",
		"command", &rstr,
		"arguments", &args);
	if (ret)
		*resp = json_pack("{ s: s, s: d }",
				"error", "invalid request",
				"code", -1);
	
	res = list_lookup(&apis, rstr);
	if (!res)
		*resp = json_pack("{ s: s, s: d, s: s }",
				"error", "command not found",
				"code", -2,
				"command", rstr);

	ret = res->cb(res, args, resp, s);
	if (ret)
		*resp = json_pack("{ s: s, s: d }",
				"error", "command failed",
				"code", ret);
	
	return 0;
}

int api_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;

	struct api_session *s = (struct api_session *) user;

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			api_session_init(s, API_MODE_WS);
			break;
		
		case LWS_CALLBACK_HTTP:
			if (len < 1) {
				lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
				return -1;
			}

			char *uri = (char *) in;

			/* Parse request URI */
			ret = sscanf(uri, "/api/v%d", (int *) &s->version);
			if (ret != 1)
				return -1;
						
			debug(LOG_API, "New REST API session initiated: version = %d", s->version);
			
			api_session_init(s, API_MODE_HTTP);

			/* Prepare HTTP response header */
			s->response.headers.buf = alloc(512);
			
			unsigned char *p = (unsigned char *) s->response.headers.buf;
			unsigned char *e = (unsigned char *) s->response.headers.buf + 512;

			ret = lws_add_http_header_status(wsi, 200, &p, e);
			if (ret)
				return 1;
			ret = lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_SERVER, (unsigned char *) USER_AGENT, strlen(USER_AGENT), &p, e);
			if (ret)
				return 1;
			ret = lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, (unsigned char *) "application/json", strlen("application/json"), &p, e);
			if (ret)
				return 1;
			//ret = lws_add_http_header_content_length(wsi, file_len, &p, e);
			//if (ret)
			//	return 1;
			ret = lws_finalize_http_header(wsi, &p, e);
			if (ret)
				return 1;
			
			*p = '\0';
			
			s->response.headers.len = p - (unsigned char *) s->response.headers.buf + 1;
			s->response.headers.sent = 0;

			/* book us a LWS_CALLBACK_HTTP_WRITEABLE callback */
			lws_callback_on_writable(wsi);

			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
		case LWS_CALLBACK_HTTP_BODY: {
			char *newbuf;
			
			newbuf = realloc(s->request.body.buf, s->request.body.len + len);
			
			s->request.body.buf = memcpy(newbuf +  s->request.body.len, in, len);
			s->request.body.len += len;
			
			json_t *req, *resp;
			while (parse_request(&s->request.body, &req) == 1) {

				api_session_run_command(s, req, &resp);

				unparse_response(&s->response.body, resp);
			}
			
			break;
		}
			
		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
			return -1; /* close connection */

		case LWS_CALLBACK_SERVER_WRITEABLE:
		case LWS_CALLBACK_HTTP_WRITEABLE: {
			enum lws_write_protocol prot;
			struct api_buffer *protbuf;
			
			if (s->mode == API_MODE_HTTP && s->response.headers.sent < s->response.headers.len) {
				prot = LWS_WRITE_HTTP_HEADERS;
				protbuf = &s->response.headers;
			}
			else if (s->response.body.sent < s->response.body.len) {
				prot = LWS_WRITE_HTTP;
				protbuf = &s->response.body;
			}
			else
				break;

			int sent;

			void *buf = (void *) (protbuf->buf + protbuf->sent);
			size_t len = protbuf->len - protbuf->sent;

			sent = lws_write(wsi, buf, len, prot);
			if (sent > 0)
				protbuf->sent += sent;
			else
				return -1;
		
			break;
		}

		default:
			return 0;
	}

	return 0;

}

int api_init(struct api *a, struct cfg *cfg)
{
	list_init(&a->sessions);

	a->cfg = cfg;

	return 0;
}

int api_deinit(struct api *a)
{
	list_destroy(&a->sessions, (dtor_cb_t) api_session_destroy, false);
	
	return 0;
}

int api_session_init(struct api_session *s, enum api_mode m)
{
	s->mode = m;
	
	s->request.body = 
	s->response.body =
	s->response.headers = (struct api_buffer) { 0 };
	
	return 0;
}

int api_session_destroy(struct api_session *s)
{
	return 0;
}
