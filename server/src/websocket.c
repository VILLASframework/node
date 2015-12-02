#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <signal.h>

#include <libwebsockets.h>

#include "websocket.h"
#include "timing.h"
#include "utils.h"
#include "msg.h"

struct msg m;

int newdata = 0;
char *resource_path = NULL;

static char * get_mimetype(const char *resource_path)
{
	char *extension = strrchr(resource_path, '.');
       
	// choose mime type based on the file extension
	if (extension == NULL)
		return "text/plain";
	else if (!strcmp(extension, ".png"))
		return "image/png";
	else if (!strcmp(extension, ".jpg"))
		return "image/jpg";
	else if (!strcmp(extension, ".gif"))
		return "image/gif";
	else if (!strcmp(extension, ".html"))
		return "text/html";
	else if (!strcmp(extension, ".css"))
		return "text/css";
	else if (!strcmp(extension, ".js"))
		return "application/javascript";
	else
		return "text/plain";
}

static int protocol_callback_http(struct libwebsocket_context *context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len)
{
	int n;

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		if (!resource_path) {
			libwebsockets_return_http_status(context, wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL);
			goto try_to_reuse;
		}

		if (len < 1) {
			libwebsockets_return_http_status(context, wsi, HTTP_STATUS_BAD_REQUEST, NULL);
			goto try_to_reuse;
		}

		char *requested_uri = (char *) in;

		/* Handle default path */
        if (!strcmp(requested_uri, "/")) {
		    void *universal_response = "HTTP/1.1 302 Found\r\nContent-Length: 0\r\nLocation: /index.html\r\n\r\ntest\r\n";
            libwebsocket_write(wsi, universal_response, strlen(universal_response), LWS_WRITE_HTTP);
			
			goto try_to_reuse;
        }
		else {
			char buf[4069];
			snprintf(buf, sizeof(buf), "%s%s", resource_path, requested_uri);
	
			/* refuse to serve files we don't understand */
			char *mimetype = get_mimetype(buf);
			if (!mimetype) {
				lwsl_err("Unknown mimetype for %s\n", buf);
				libwebsockets_return_http_status(context, wsi, HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL);
				return -1;
			}

			n = libwebsockets_serve_http_file(context, wsi, buf, mimetype, NULL, 0);
			if (n < 0) {
				lwsl_warn("Failed to serve: %s", resource_path);
				return -1;
			}
			if (n == 0)
				break;
		}
		
		goto try_to_reuse;

	default:
		break;
	}

	return 0;
	
try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}

static int protocol_callback_live(struct libwebsocket_context *context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len)
{	
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED: {
			char name[1024];
			char rip[1024];

			int fd = libwebsocket_get_socket_fd(wsi);
			libwebsockets_get_peer_addresses(context, wsi, fd, name, sizeof(name), rip, sizeof(rip));
		
	        lwsl_notice("New Connection from: %s %s", name, rip);
			break;
		}
		
		case LWS_CALLBACK_SERVER_WRITEABLE: {	
			char *data = NULL, *buf;
			int len;
			
			strcatf(&data, "%f", time_to_double(&MSG_TS(&m)));
			for (int i = 0; i < m.length; i++)
				strcatf(&data, " %f", m.data[i].f);
			
			len = strlen(data);
			buf = malloc(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING);
			
			memcpy(buf + LWS_SEND_BUFFER_PRE_PADDING, data, len);
			free(data);
			
			libwebsocket_write(wsi, (unsigned char *) &buf[LWS_SEND_BUFFER_PRE_PADDING], len, LWS_WRITE_TEXT);
			break;
		}

		case LWS_CALLBACK_RECEIVE: {
			lwsl_notice("Received data: %s", (char *) in);

			break;
		}

		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: {
			char buf[1024];
			
			lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_GET_URI);
			lwsl_notice("WSI_TOKEN_GET_URI = %s", buf);
			
			break;
		}
		

		default:
			break;
	}

	return 0;
}

static struct libwebsocket_protocols protocols[] = {
    { "http-only", protocol_callback_http,  0, 0 },
	{ "live", protocol_callback_live, 30, 10 },
	{ NULL, NULL, 0, 0 } /* terminator */
};


static void lws_debug(int level,  const char *msg) {
	int len = strlen(msg);
	if (strchr(msg, '\n'))
		len -= 1;
		
	switch (level) {
		case LLL_ERR:		error("lws: %.*s", len, msg); break;
		case LLL_WARN:		warn("lws: %.*s", len, msg);  break;
		case LLL_INFO:		info("lws: %.*s", len, msg);  break;
		default:			debug(1, "lws: %.*s", len, msg);
	}
}

static void * lws_thread(void *ctx)
{
	struct websocket *w = ctx;
	
	int n;
	do {
		n = libwebsocket_service(w->context, 10);
	} while (n >= 0);
	
	return NULL;
}

int websocket_init(int argc, char * argv[], struct settings *set)
{
	lws_set_log_level((1 << LLL_COUNT) - 1, lws_debug);

	return 0;
}

int websocket_deinit()
{
	return 0;
}

int websocket_parse(config_setting_t *cfg, struct node *n)
{
	struct websocket *w = n->websocket;
	
	config_lookup_string(cfg, "ssl_cert", &w->ssl_cert);
	config_lookup_string(cfg, "ssl_private_key", &w->ssl_private_key);
	config_lookup_string(cfg, "htdocs", &w->htdocs);
	
	if (!config_lookup_int(cfg, "port", &w->port))
		w->port = 80;

}

char * websocket_print(struct node *n)
{
	struct websocket *w = n->websocket;
	char *buf = NULL;
	
	return strcatf(&buf, "port=%u", w->port);
}

int websocket_open(struct node *n)
{
	struct websocket *w = n->websocket;
	
	struct lws_context_creation_info info;
	
	memset(&info, 0, sizeof info);
	info.port = w->port;
	info.iface = NULL;
	info.protocols = protocols;
	info.extensions = libwebsocket_get_internal_extensions();
	info.ssl_cert_filepath = w->ssl_cert;
	info.ssl_private_key_filepath = w->ssl_private_key;
	info.gid = -1;
	info.uid = -1;
	info.options = 0;

	w->context = libwebsocket_create_context(&info);
	if (w->context == NULL) {
		lwsl_notice("init failed");
		return -1;
	}
	
	pthread_create(w->thread, NULL, lws_thread, w);

	return 0;
}

int websocket_close(struct node *n)
{
	struct websocket *w = n->websocket;
	
	libwebsocket_cancel_service(w->context);
	libwebsocket_context_destroy(w->context);
	
	pthread_join(w->thread, NULL);

	lwsl_notice("exited cleanly");
	
	return 0;
}

int websocket_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	return 0;
}

int websocket_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	return 0;
}

REGISTER_NODE_TYPE(WEBSOCKET, "websocket", websocket)
