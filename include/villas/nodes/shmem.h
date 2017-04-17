/** Node-type for shared memory communication.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

char *shmem_print(struct node *n);

int shmem_parse(struct node *n, config_setting_t *cfg);

int shmem_open(struct node *n);

int shmem_close(struct node *n);

int shmem_read(struct node *n, struct sample *smps[], unsigned cnt);

int shmem_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
