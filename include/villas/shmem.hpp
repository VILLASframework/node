/** Shared-memory interface: The interface functions that the external program should use.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/pool.hpp>
#include <villas/queue.h>
#include <villas/queue_signalled.h>
#include <villas/sample.hpp>

#define DEFAULT_SHMEM_QUEUELEN	512u
#define DEFAULT_SHMEM_SAMPLELEN	64u

namespace villas {
namespace node {

/** Struct containing all parameters that need to be known when creating a new
 * shared memory object. */
struct ShmemConfig {
	int polling;			/**< Whether to use polling instead of POSIX CVs */
	int queuelen;			/**< Size of the queues (in elements) */
	int samplelen;			/**< Maximum number of data entries in a single sample */
};

/** The structure that actually resides in the shared memory. */
struct ShmemShared {
	int polling;			/**< Whether to use a pthread_cond_t to signal if new samples are written to incoming queue. */
	struct CQueueSignalled queue;	/**< Queue for samples passed in both directions. */
	struct Pool pool;		/**< Pool for the samples in the queues. */
};

/** Relevant information for one direction of the interface. */
struct shmem_dir {
	void *base;			/**< Base address of the region. */
	const char *name;		/**< Name of the shmem object. */
	size_t len;			/**< Total size of the region. */
	struct ShmemShared *shared;	/**< Actually shared datastructure */
};

/** Main structure representing the shared memory interface. */
struct ShmemInterface {
	struct shmem_dir read, write;
	std::atomic<int> readers, writers, closed;
};

/** Open the shared memory objects and retrieve / initialize the shared data structures.
 * Blocks until another process connects by opening the same objects.
 *
 * @param[in] wname Name of the POSIX shared memory object containing the output queue.
 * @param[in] rname Name of the POSIX shared memory object containing the input queue.
 * @param[inout] shm The shmem_int structure that should be used for following
 * calls will be written to this pointer.
 * @param[in] conf Configuration parameters for the output queue.
 * @retval 0 The objects were opened and initialized successfully.
 * @retval <0 An error occured; errno is set accordingly.
 */
int shmem_int_open(const char* wname, const char* rname, struct ShmemInterface* shm, struct ShmemConfig* conf);

/** Close and destroy the shared memory interface and related structures.
 *
 * @param shm The shared memory interface.
 * @retval 0 Closing successfull.
 * @retval <0 An error occurred; errno is set appropiately.
 */
int shmem_int_close(struct ShmemInterface *shm);

/** Read samples from the interface.
 *
 * @param shm The shared memory interface.
 * @param smps  An array where the pointers to the samples will be written. The samples
 * must be freed with sample_decref after use.
 * @param cnt  Number of samples to be read.
 * @retval >=0 Number of samples that were read. Can be less than cnt (including 0) in case not enough samples were available.
 * @retval -1 The other process closed the interface; no samples can be read anymore.
 */
int shmem_int_read(struct ShmemInterface *shm, struct Sample * const smps[], unsigned cnt);

/** Write samples to the interface.
 *
 * @param shm The shared memory interface.
 * @param smps The samples to be written. Must be allocated from shm_int_alloc.
 * @param cnt Number of samples to write.
 * @retval >=0 Number of samples that were successfully written. Can be less than cnt (including 0) in case of a full queue.
 * @retval -1 The write failed for some reason; no more samples can be written.
 */
int shmem_int_write(struct ShmemInterface *shm, const struct Sample * const smps[], unsigned cnt);

/** Allocate samples to be written to the interface.
 *
 * The writing process must not free the samples; only the receiving process should free them using sample_decref after use.
 * @param shm The shared memory interface.
 * @param smps Array where pointers to newly allocated samples will be returned.
 * @param cnt Number of samples to allocate.
 * @return Number of samples that were successfully allocated (may be less then cnt).
 */
int shmem_int_alloc(struct ShmemInterface *shm, struct Sample *smps[], unsigned cnt);

/** Returns the total size of the shared memory region with the given size of
 * the input/output queues (in elements) and the given number of data elements
 * per struct Sample. */
size_t shmem_total_size(int queuelen, int samplelen);

} /* namespace node */
} /* namespace villas */
