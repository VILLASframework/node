/** Node type: ethercat
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

/**
 * @addtogroup ethercats WebSockets node type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node.h>
#include <villas/pool.h>
#include <villas/queue_signalled.h>
#include <villas/common.h>
#include <villas/io.h>
#include <villas/config.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_ETHERCAT_QUEUE_LENGTH	(DEFAULT_QUEUE_LENGTH * 64)
#define DEFAULT_ETHERCAT_SAMPLE_LENGTH	DEFAULT_SAMPLE_LENGTH

/* Forward declaration */
struct lws;

/** Internal data per ethercat node */
struct ethercat {
	struct pool pool;
	struct queue_signalled queue;		/**< For samples which are received from WebSockets */
};

/* Internal datastructures */


/** @see node_type::type_start */
int ethercat_type_start(struct super_node *sn);

/** @see node_type::type_stop */
int ethercat_type_stop();

/** @see node_type::open */
int ethercat_start(struct node *n);

/** @see node_type::close */
int ethercat_stop(struct node *n);

/** @see node_type::close */
int ethercat_destroy(struct node *n);

/** @see node_type::read */
int ethercat_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::write */
int ethercat_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @} */

#ifdef __cplusplus
}
#endif
