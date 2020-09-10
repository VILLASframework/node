/** Wrapper around queue that uses POSIX CV's for signalling writes.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <pthread.h>

#include <villas/queue.h>

enum class QueueSignalledMode {
	AUTO, /**< We will choose the best method available on the platform */
	PTHREAD,
	POLLING,
#ifdef HAS_EVENTFD
	EVENTFD,
#elif defined(__APPLE__)
	PIPE,
#endif
};

enum class QueueSignalledFlags {
	PROCESS_SHARED	= (1 << 4)
};

/** Wrapper around queue that uses POSIX CV's for signalling writes. */
struct queue_signalled {
	struct queue queue;		/**< Actual underlying queue. */

	enum QueueSignalledMode mode;
	enum QueueSignalledFlags flags;

	union {
		struct {
			pthread_cond_t ready;		/**< Condition variable to signal writes to the queue. */
			pthread_mutex_t mutex;		/**< Mutex for ready. */
		} pthread;
#ifdef __linux__
		int eventfd;
#elif defined(__APPLE__)
		int pipe[2];
#endif
	};
};

#define queue_signalled_available(q) queue_available(&((q)->queue))

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memory_type *mem = memory_default, enum QueueSignalledMode mode = QueueSignalledMode::AUTO, int flags = 0) __attribute__ ((warn_unused_result));

int queue_signalled_destroy(struct queue_signalled *qs) __attribute__ ((warn_unused_result));

int queue_signalled_push(struct queue_signalled *qs, void *ptr) __attribute__ ((warn_unused_result));

int queue_signalled_pull(struct queue_signalled *qs, void **ptr) __attribute__ ((warn_unused_result));

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt) __attribute__ ((warn_unused_result));

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt) __attribute__ ((warn_unused_result));

int queue_signalled_close(struct queue_signalled *qs) __attribute__ ((warn_unused_result));

/** Returns a file descriptor which can be used with poll / select to wait for new data */
int queue_signalled_fd(struct queue_signalled *qs);
