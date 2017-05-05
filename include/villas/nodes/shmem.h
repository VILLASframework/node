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

#include "node.h"
#include "memory.h"
#include "pool.h"
#include "queue.h"
#include "config.h"

#define DEFAULT_SHMEM_QUEUELEN	512
#define DEFAULT_SHMEM_SAMPLELEN	DEFAULT_SAMPLELEN

/** Node-type for shared memory communication.
 * @see node_type
 */
struct shmem {
	const char* name;		/**< Name of the shm object. */
	int samplelen;			/**< Number of data entries for each sample. */
	int queuelen;			/**< Size of ingoing and outgoing queue, respectively. */
	int polling;			/**< Whether to use a pthread_cond_t to signal if new samples are written to outgoing queue. */
	char **exec;			/**< External program to execute on start. */

	struct memtype *manager;	/**< Manager for the shared memory region. */
	int fd;				/**< Handle as returned by shm_open().*/
	void *base;			/**< Pointer to the shared memory region. */
	struct shmem_shared *shared;	/**< Shared datastructure. */
};

/** @see node_type::print */
char * shmem_print(struct node *n);

/** @see node_type::parse */
int shmem_parse(struct node *n, config_setting_t *cfg);

/** @see node_type::open */
int shmem_open(struct node *n);

/** @see node_type::close */
int shmem_close(struct node *n);

/** @see node_type::read */
int shmem_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int shmem_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
