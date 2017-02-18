/** LWS-releated functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#pragma once

/* Forward declarations */
struct api;

struct web {
	struct api *api;
	
	struct lws_context *context;	/**< The libwebsockets server context. */
	struct lws_vhost *vhost;	/**< The libwebsockets vhost. */

	int port;			/**< Port of the build in HTTP / WebSocket server. */
	const char *htdocs;		/**< The root directory for files served via HTTP. */
	const char *ssl_cert;		/**< Path to the SSL certitifcate for HTTPS / WSS. */
	const char *ssl_private_key;	/**< Path to the SSL private key for HTTPS / WSS. */
};

/** Initialize the web interface.
 *
 * The web interface is based on the libwebsockets library.
 */
int web_init(struct web *w, struct api *a);

int web_destroy(struct web *w);

/** Parse HTTPd and WebSocket related options */
int web_parse(struct web *w, config_setting_t *lcs);

/** De-initializes the web interface. */
int web_deinit(struct web *w);

/** libwebsockets service routine. Call periodically */
int web_service(struct web *w);