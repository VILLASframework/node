/** Node type: IEC 61850-9-2 (Sampled Values)
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdint>

#include <libiec61850/sv_publisher.h>
#include <libiec61850/sv_subscriber.h>

#include <villas/node.hpp>
#include <villas/queue_signalled.h>
#include <villas/pool.hpp>
#include <villas/nodes/iec61850.hpp>

namespace villas {
namespace node {
namespace iec61850 {

class SVNode : public Node {

protected:

	std::string interface;
	int app_id;
	struct ether_addr dst_address;

	struct {
		bool enabled;

		::SVSubscriber subscriber;
		SVReceiver *receiver;

		struct CQueueSignalled queue;
		struct Pool pool;

		std::vector<const TypeDescriptor *> signals;
		int total_size;
	} in;

	struct {
		bool enabled;

		::SVPublisher publisher;
		::SVPublisher_ASDU asdu;

		std::string svid;

		int vlan_priority;
		int vlan_id;
		int smpmod;
		int smprate;
		int confrev;

		std::vector<const TypeDescriptor *> signals;
		int total_size;
	} out;

	virtual
	int _read(struct Sample * smps[], unsigned cnt);

	virtual
	int _write(struct Sample * smps[], unsigned cnt);

	static
	void listenerStatic(SVSubscriber subscriber, void *ctx, SVSubscriber_ASDU asdu);

	void listener(SVSubscriber subscriber, SVSubscriber_ASDU asdu);

public:
	SVNode(const std::string &name = "");

	virtual
	~SVNode();

	virtual
	int start();

	virtual
	int stop();

	virtual
	std::vector<int> getPollFDs();

	virtual
	std::string getDetails();

	virtual
	int parse(json_t *json, const uuid_t sn_uuid);
};

int iec61850_sv_type_stop();

} /* namespace iec61850 */
} /* namespace node */
} /* namespace villas */
