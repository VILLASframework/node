/** HTTP protocol of the websocket node type
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <libwebsockets.h>
#include <libconfig.h>

#ifdef WITH_JANSSON
  #include <jansson.h>
#endif

/* Choose mime type based on the file extension */
static char * get_mimetype(const char *resource_path)
{
	char *extension = strrchr(resource_path, '.');

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

static int protocol_cb_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	switch (reason) {
		case LWS_CALLBACK_HTTP:			
			if (!htdocs) {
				lws_return_http_status(wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL);
				goto try_to_reuse;
			}

			if (len < 1) {
				lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
				goto try_to_reuse;
			}

			char *requested_uri = (char *) in;
			
			debug(DBG_WEBSOCKET | 3, "WebSocket: New HTTP request: %s", requested_uri);

			/* Handle default path */
			if      (!strcmp(requested_uri, "/")) {
				char *response = "HTTP/1.1 302 Found\r\n"
						 "Content-Length: 0\r\n"
						 "Location: /index.html\r\n"
						 "\r\n";
			
				lws_write(wsi, (void *) response, strlen(response), LWS_WRITE_HTTP);
			
				goto try_to_reuse;
			}
#ifdef WITH_JANSSON
			/* Return list of websocket nodes */
			else if (!strcmp(requested_uri, "/nodes.json")) {
				json_t *json_body = json_array();
								
				list_foreach(struct node *n, &vt.instances) {
					struct websocket *w = n->_vd;

					json_t *json_node = json_pack("{ s: s, s: i, s: i, s: i, s: i }",
						"name",		node_name_short(n),
						"connections",	list_length(&w->connections),
						"state",	n->state,
						"vectorize",	n->vectorize,
						"affinity",	n->affinity
					);
					
					/* Add all additional fields of node here.
					 * This can be used for metadata */	
					json_object_update(json_node, config_to_json(n->cfg));
					
					json_array_append_new(json_body, json_node);
				}
				
				char *body = json_dumps(json_body, JSON_INDENT(4));
					
				char *header =  "HTTP/1.1 200 OK\r\n"
						"Connection: close\r\n"
       						"Content-Type: application/json\r\n"
						"\r\n";
				
				lws_write(wsi, (void *) header, strlen(header), LWS_WRITE_HTTP);
				lws_write(wsi, (void *) body,   strlen(body),   LWS_WRITE_HTTP);

				free(body);
				json_decref(json_body);
				
				return -1;
			}
			else if (!strcmp(requested_uri, "/config.json")) {
				char *body = json_dumps(config_to_json(cfg_root), JSON_INDENT(4));
					
				char *header =  "HTTP/1.1 200 OK\r\n"
						"Connection: close\r\n"
       						"Content-Type: application/json\r\n"
						"\r\n";
				
				lws_write(wsi, (void *) header, strlen(header), LWS_WRITE_HTTP);
				lws_write(wsi, (void *) body,   strlen(body),   LWS_WRITE_HTTP);

				free(body);
				
				return -1;
			}
#endif
			else {
				char path[4069];
				snprintf(path, sizeof(path), "%s%s", htdocs, requested_uri);

				/* refuse to serve files we don't understand */
				char *mimetype = get_mimetype(path);
				if (!mimetype) {
					warn("HTTP: Unknown mimetype for %s", path);
					lws_return_http_status(wsi, HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL);
					return -1;
				}

				int n = lws_serve_http_file(wsi, path, mimetype, NULL, 0);
				if      (n < 0)
					return -1;
				else if (n == 0)
					break;
				else
					goto try_to_reuse;
			}

		default:
			break;
	}

	return 0;
	
try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}