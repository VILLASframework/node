/** Node type: IEC 61850-9-2 (Sampled Values)
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <cstdint>

#include <libiec61850/sv_publisher.h>
#include <libiec61850/sv_subscriber.h>

#include <villas/queue_signalled.h>
#include <villas/pool.hpp>
#include <villas/list.hpp>
#include <villas/nodes/iec61850.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct iec61850_sv {
	char *interface;
	int app_id;
	struct ether_addr dst_address;

	struct {
		bool enabled;

		SVSubscriber subscriber;
		SVReceiver receiver;

		struct CQueueSignalled queue;
		struct Pool pool;

		struct List signals;		/**< Mappings of type struct iec61850_type_descriptor */
		int total_size;
	} in;

	struct {
		bool enabled;

		SVPublisher publisher;
		SVPublisher_ASDU asdu;

		char *svid;

		int vlan_priority;
		int vlan_id;
		int smpmod;
		int smprate;
		int confrev;

		struct List signals;		/**< Mappings of type struct iec61850_type_descriptor */
		int total_size;
	} out;
};

int iec61850_sv_type_stop();

int iec61850_sv_parse(NodeCompat *n, json_t *json);

char * iec61850_sv_print(NodeCompat *n);

int iec61850_sv_start(NodeCompat *n);

int iec61850_sv_stop(NodeCompat *n);

int iec61850_sv_destroy(NodeCompat *n);

int iec61850_sv_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int iec61850_sv_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int iec61850_sv_poll_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
