/** Node type: ZeroMQ
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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
 * @addtogroup zeromq ZeroMQ node type
 * @ingroup node
 * @{
 */

#pragma once

#include <stdint.h>

#include "node.h"
#include "list.h"

#if ZMQ_VERSION_MAJOR > 4 || (ZMQ_VERSION_MAJOR == 4 && ZMQ_VERSION_MINOR >= 2)
  #define ZMQ_BUILD_DISH 1
#endif

struct zeromq {
	int ipv6;

	char *filter;

	struct {
		int enabled;
		struct {
			char public_key[41];
			char secret_key[41];
		} server, client;
	} curve;

	enum {
		ZEROMQ_PATTERN_PUBSUB,
#ifdef ZMQ_BUILD_DISH
		ZEROMQ_PATTERN_RADIODISH
#endif
	} pattern;

	struct {
		void *socket;	/**< ZeroMQ socket. */
		void *mon_socket;
		char *endpoint;
	} subscriber;

	struct {
		void *socket;	/**< ZeroMQ socket. */
		struct list endpoints;
	} publisher;
};

/** @see node_type::print */
char * zeromq_print(struct node *n);

/** @see node_type::parse */
int zeromq_parse(struct node *n, json_t *cfg);

/** @see node_type::init */
int zeromq_init();

/** @see node_type::deinit */
int zeromq_deinit();

/** @see node_type::open */
int zeromq_start(struct node *n);

/** @see node_type::close */
int zeromq_stop(struct node *n);

/** @see node_type::read */
int zeromq_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int zeromq_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
