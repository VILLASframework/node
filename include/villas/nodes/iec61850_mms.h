/** Node type: IEC 61850-8-1 (MMS)
 *
 * @author Iris Koester <ikoester@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/queue_signalled.h>
#include <libiec61850/mms_client_connection.h>
#include <villas/node.h>
#include <villas/pool.h>
#include <villas/nodes/iec61850.h>
#include <villas/sample.h>
#include <villas/task.h>


struct iec61850_mms {
	struct task task;	/**< timer for periodic events */
	char * host;		/**< hostname / IP address of MMS Server */
	int port;		/**< TCP port of MMS Server */

	int rate;		/**< sampling rate */
	int counter;		/**< number of samples already transmitted */

	char * domainID;	/**< Domain ID of the to-be-read item */
	char * itemID;		/**< item ID (name of MMS value) */

	MmsConnection conn;	/**< Connection instance to MMS Server */

	struct {
		struct list iecTypeList;	/**< mappings of type struct iec61850_type_descriptor */
		struct list domain_ids;		/**< list of const char *, contains domainIDs for MMS values */
		struct list item_ids;		/**< list of const char *, contains itemIDs for MMS values */

		int totalsize;			/**< length of all lists: iecType, domainIDs, itemIDs */
	} in;

	struct {
		bool isTest;
		int testvalue;
		struct list iecTypeList;	/**< mappings of type struct iec61850_type_descriptor */
		struct list domain_ids;		/**< list of const char *, contains domainIDs for MMS values */
		struct list item_ids;		/**< list of const char *, contains itemIDs for MMS values */

		int totalsize;			/**< length of all lists: iecType, domainIDs, itemIDs */
	} out;
};

/** Parse MMS configuration parameters
  *
  *@param mms_ids JSON object that contains pairs of domain and item IDs
  *@param domainIDs pointer to list into which the domain IDs will be written
  *@param itemIDs pointer to list into which the item IDs will be written
  *
  *@return length the lists
  */
int iec61850_mms_parse_ids(json_t * mms_ids, struct list * domainIDs, struct list * itemIDs);
