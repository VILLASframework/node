/** Node type: OMA Next Generation Services Interface 10 (NGSI) (FIWARE context broker)
 *
 * This file implements the NGSI context interface. NGSI is RESTful HTTP is specified by
 * the Open Mobile Alliance (OMA).
 * It uses the standard operations of the NGSI 10 context information standard.
 *
 * @see https://forge.fiware.org/plugins/mediawiki/wiki/fiware/index.php/FI-WARE_NGSI-10_Open_RESTful_API_Specification
 * @see http://technical.openmobilealliance.org/Technical/Release_Program/docs/NGSI/V1_0-20120529-A/OMA-TS-NGSI_Context_Management-V1_0-20120529-A.pdf
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */
/**
 * @addtogroup ngsi FIRWARE NGSI 9/10 RESTful HTTP API
 * @ingroup node
 * @{
 **********************************************************************************/

#ifndef _NGSI_H_
#define _NGSI_H_

#include <curl/curl.h>
#include <jansson.h>

#include "list.h"
#include "config.h"
#include "msg.h"
#include "cfg.h"
#include "node.h"

struct node;

struct ngsi {
	const char *endpoint;		/**< The NGSI context broker endpoint URL. */
	const char *token;		/**< An optional authentication token which will be sent as HTTP header. */

	double timeout;			/**< HTTP timeout in seconds */

	int ssl_verify;			/**< Boolean flag whether SSL server certificates should be verified or not. */ 


	enum ngsi_structure {
		NGSI_FLAT,
		NGSI_CHILDREN
	} structure;			/**< Structure of published entitites */
	

	/** A mapping between indices of the S2SS messages and the attributes in ngsi::context */
	json_t **context_map;
	/** The number of mappings in ngsi::context_map */
	int context_len;
	struct curl_slist *headers;	/**< List of HTTP request headers for libcurl */

	CURL *curl;			/**< libcurl: handle */

	json_t *context;		/**< The complete JSON tree which will be used for contextUpdate requests */
};

/** Initialize global NGSI settings and maps shared memory regions.
 *
 * @see node_vtable::init
 */
int ngsi_init(int argc, char *argv[], struct settings *set);

/** Free global NGSI settings and unmaps shared memory regions.
 *
 * @see node_vtable::deinit
 */
int ngsi_deinit();

/** @see node_vtable::parse */
int ngsi_parse(config_setting_t *cfg, struct node *n);

/** @see node_vtable::print */
char * ngsi_print(struct node *n);

/** @see node_vtable::open */
int ngsi_open(struct node *n);

/** @see node_vtable::close */
int ngsi_close(struct node *n);

/** @see node_vtable::read */
int ngsi_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** @see node_vtable::write */
int ngsi_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

#endif /** _NGSI_H_ @} */