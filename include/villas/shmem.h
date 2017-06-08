/** Shared-memory interface: The interface functions that the external program should use.
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

/** Interface to the shared memory node for external programs.
 *
 * @addtogroup shmem Shared memory interface
 * @{
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "pool.h"
#include "queue.h"
#include "queue_signalled.h"
#include "sample.h"

#define DEFAULT_SHMEM_QUEUELEN	512
#define DEFAULT_SHMEM_SAMPLELEN	DEFAULT_SAMPLELEN

/** A signalled queue or a regular (polling) queue, depending on the polling setting. */
union shmem_queue {
	struct queue q;
	struct queue_signalled qs;
};

#define SHMEM_MIN_SIZE (sizeof(struct memtype) + sizeof(struct memblock) + sizeof(pthread_barrier_t) + sizeof(pthread_barrierattr_t))

/** Struct containing all parameters that need to be known when creating a new
 * shared memory object. */
struct shmem_conf {
    int polling; /*< Whether to use polling instead of POSIX CVs */
    int queuelen; /*< Size of the queues (in elements) */
    int samplelen; /*< Maximum number of data entries in a single sample */
};

/** The structure that actually resides in the shared memory. */
struct shmem_shared {
	pthread_barrier_t start_bar;      /**< Barrier for synchronizing the start of both programs. */

	int polling;                      /**< Whether to use a pthread_cond_t to signal if new samples are written to incoming queue. */

	union shmem_queue queue[2];       /**< Queues for samples passed in both directions.
                                           0: primary -> secondary, 1: secondary -> primary */

	struct pool pool;                 /**< Pool for the samples in the queues. */
};

struct shmem_int {
    void* base; /**< Base address of the mapping (needed for munmap) */
    const char* name; /**< Name of the shared memory object */
    size_t len; /**< Total size of the shared memory region */
    struct shmem_shared *shared; /**< Pointer to mapped shared structure */
    int secondary; /**< Set to 1 if this is the secondary user (i.e. not the one
                        that created the object); 0 otherwise. */
};

/** Open the shared memory object and retrieve / initialize the shared data structures.
 * If the object isn't already present, it is created instead.
 * @param[in] name Name of the POSIX shared memory object.
 * @param[inout] shm The shmem_int structure that should be used for following
 * calls will be written to this pointer.
 * @param[in] conf Configuration parameters for the interface. This struct is
 * ignored if the shared memory object is already present.
 * @retval 1 The object was created successfully.
 * @retval 0 The existing object was opened successfully.
 * @retval <0 An error occured; errno is set accordingly.
 */
int shmem_int_open(const char* name, struct shmem_int* shm, struct shmem_conf* conf);

/** Close and destroy the shared memory interface and related structures.
 * @param shm The shared memory interface.
 * @retval 0 Closing successfull.
 * @retval <0 An error occurred; errno is set appropiately.
 */
int shmem_int_close(struct shmem_int *shm);

/** Read samples from the interface.
 * @param shm The shared memory interface.
 * @param smps  An array where the pointers to the samples will be written. The samples
 * must be freed with sample_put after use.
 * @param cnt  Number of samples to be read.
 * @retval >=0 Number of samples that were read. Can be less than cnt (including 0) in case not enough samples were available.
 * @retval -1 The other process closed the interface; no samples can be read anymore.
 */
int shmem_int_read(struct shmem_int *shm, struct sample *smps[], unsigned cnt);

/** Write samples to the interface.
 * @param shm The shared memory interface.
 * @param smps The samples to be written. Must be allocated from shm->pool.
 * @param cnt Number of samples to write.
 * @retval >=0 Number of samples that were successfully written. Can be less than cnt (including 0) in case of a full queue.
 * @retval -1 The write failed for some reason; no more samples can be written.
 */
int shmem_int_write(struct shmem_int *shm, struct sample *smps[], unsigned cnt);

/** Returns the total size of the shared memory region with the given size of
 * the input/output queues (in elements) and the given number of data elements
 * per struct sample. */
size_t shmem_total_size(int insize, int outsize, int sample_size);

/** @} */

#ifdef __cplusplus
}
#endif
