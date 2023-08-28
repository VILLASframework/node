/** Node type: ethercat
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <post@steffenvogel.de>
 * @author Divya Laxetti <divya.laxetti@rwth-aachen.de>
 * @copyright 2018-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <thread>

#include <villas/pool.hpp>
#include <villas/task.hpp>
#include <villas/queue_signalled.h>
#include <villas/common.hpp>
#include <villas/format.hpp>
#include <villas/config.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;
class SuperNode;

/* Include hard-coded Ethercat Bus configuration */
#include <villas/nodes/ethercat_config.hpp>

extern "C" {
	#include <ecrt.h>
}

#define DEFAULT_ETHERCAT_QUEUE_LENGTH	(DEFAULT_QUEUE_LENGTH * 64)

/** Internal data per ethercat node */
struct ethercat {
	/* Settings */
	double rate;

	struct {
		unsigned num_channels;
		double range;
		unsigned position;
		unsigned product_code;		/**< Product ID of EtherCAT slave */
		unsigned vendor_id;		/**< Vendor ID of EtherCAT slave */

		ec_slave_config_t *sc;
		unsigned *offsets;		/**< Offsets for PDO entries */
	} in, out;

	ec_domain_t *domain;
	ec_pdo_entry_reg_t *domain_regs;
	uint8_t *domain_pd;			/**< Process data */

	std::thread thread;			/**< Cyclic task thread */
	struct Task task;			/**< Periodic timer */
	struct Pool pool;
	struct CQueueSignalled queue;		/**< For samples which are received from WebSockets */

	std::atomic<struct Sample *> send;	/**< Last sample to be sent via EtherCAT */
};

/* Internal datastructures */

int ethercat_type_start(SuperNode *sn);

int ethercat_type_stop();

int ethercat_init(NodeCompat *n);

int ethercat_destroy(NodeCompat *n);

int ethercat_parse(NodeCompat *n, json_t *json);

int ethercat_check(NodeCompat *n);

int ethercat_prepare(NodeCompat *n);

int ethercat_start(NodeCompat *n);

int ethercat_stop(NodeCompat *n);

int ethercat_poll_fds(NodeCompat *n, int fds[]);

char * ethercat_print(NodeCompat *n);

int ethercat_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int ethercat_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} // namespace node
} // namespace villas
