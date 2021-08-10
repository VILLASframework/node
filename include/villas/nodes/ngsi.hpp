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
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <curl/curl.h>
#include <jansson.h>

#include <villas/list.hpp>
#include <villas/task.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct ngsi {
	const char *endpoint;		/**< The NGSI context broker endpoint URL. */
	const char *entity_id;		/**< The context broker entity id related to this node */
	const char *entity_type;	/**< The type of the entity */
	const char *access_token;	/**< An optional authentication token which will be sent as HTTP header. */

	bool create;			/**< Weather we want to create the context element during startup. */
	bool remove;			/**< Weather we want to delete the context element during startup. */

	double timeout;			/**< HTTP timeout in seconds */
	double rate;			/**< Rate used for polling. */

	struct Task task;		/**< Timer for periodic events. */
	int ssl_verify;			/**< Boolean flag whether SSL server certificates should be verified or not. */

	struct curl_slist *headers;	/**< List of HTTP request headers for libcurl */

	struct {
		CURL *curl;		/**< libcurl: handle */
		struct List signals;	/**< A mapping between indices of the VILLASnode samples and the attributes in ngsi::context */
	} in, out;
};


int ngsi_type_start(SuperNode *sn);

int ngsi_type_stop();

int ngsi_parse(NodeCompat *n, json_t *json);

char * ngsi_print(NodeCompat *n);

int ngsi_init(NodeCompat *n);

int ngsi_destroy(NodeCompat *n);

int ngsi_start(NodeCompat *n);

int ngsi_stop(NodeCompat *n);

int ngsi_reverse(NodeCompat *n);

int ngsi_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int ngsi_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int ngsi_poll_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
