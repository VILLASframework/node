/** Node-type for shared memory communication.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
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
 * @ingroup node
 * @addtogroup shmem_node Shared memory node type
 * @{
 */

#pragma once

#include <villas/node.h>
#include <villas/memory.h>
#include <villas/pool.h>
#include <villas/queue.h>
#include <villas/config.h>
#include <villas/shmem.h>

/** Node-type for shared memory communication.
 * @see node_type
 */
struct shmem {
	const char* out_name;   	/**< Name of the shm object for the output queue. */
	const char* in_name;    	/**< Name of the shm object for the input queue. */
	struct shmem_conf conf; 	/**< Interface configuration struct. */
	char **exec;            	/**< External program to execute on start. */
	struct shmem_int intf;  	/**< Shmem interface */
};

/** @see node_type::print */
char * shmem_print(struct node *n);

/** @see node_type::parse */
int shmem_parse(struct node *n, json_t *cfg);

/** @see node_type::start */
int shmem_start(struct node *n);

/** @see node_type::stop */
int shmem_stop(struct node *n);

/** @see node_type::read */
int shmem_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int shmem_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
