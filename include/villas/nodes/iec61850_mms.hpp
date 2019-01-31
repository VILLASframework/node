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
#include <villas/node.hpp>
#include <villas/pool.hpp>
#include <villas/nodes/iec61850.hpp>
#include <villas/sample.hpp>
#include <villas/task.hpp>

struct iec61850_mms_signal {
	const struct iec61850_type_descriptor *type;

	char *domain_id;	/**< contains domainIDs for MMS values */
	char *item_id;		/**< contains itemIDs for MMS values */
};

struct iec61850_mms {
	Task task;		/**< timer for periodic events */
	char *host;		/**< hostname / IP address of MMS Server */
	int port;		/**< TCP port of MMS Server */

	int rate;		/**< sampling rate */
	int counter;		/**< number of samples already transmitted */

	MmsConnection conn;	/**< Connection instance to MMS Server */

	struct {
		std::vector<struct iec61850_mms_signal> signals;

		int total_size;
	} in;

	struct {
		bool is_test;
		int testvalue;

		std::vector<struct iec61850_mms_signal> signals;

		int total_size;
	} out;
};
