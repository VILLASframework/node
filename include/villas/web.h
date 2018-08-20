/** LWS-releated functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <pthread.h>

#include <villas/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct api;

struct web {
	struct api *api;

	enum state state;

	struct lws_context *context;	/**< The libwebsockets server context. */
	struct lws_vhost *vhost;	/**< The libwebsockets vhost. */

	int port;			/**< Port of the build in HTTP / WebSocket server. */
	char *htdocs;			/**< The root directory for files served via HTTP. */
	char *ssl_cert;			/**< Path to the SSL certitifcate for HTTPS / WSS. */
	char *ssl_private_key;		/**< Path to the SSL private key for HTTPS / WSS. */

	pthread_t thread;
};

/** Initialize the web interface.
 *
 * The web interface is based on the libwebsockets library.
 */
int web_init(struct web *w, struct api *a);

int web_destroy(struct web *w);

int web_start(struct web *w);

int web_stop(struct web *w);

/** Parse HTTPd and WebSocket related options */
int web_parse(struct web *w, json_t *cfg);

#ifdef __cplusplus
}
#endif
