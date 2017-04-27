/** Wrapper around queue that uses POSIX CV's for signalling writes.
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

#pragma once

#include <pthread.h>

#include "queue.h"

/** Wrapper around queue that uses POSIX CV's for signalling writes. */
struct queue_signalled {
	struct queue queue;		/**< Actual underlying queue. */
	pthread_cond_t ready;		/**< Condition variable to signal writes to the queue. */
	pthread_mutex_t mutex;		/**< Mutex for ready. */
};

#define queue_signalled_available(q) queue_available(&((q)->queue))

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memtype *mem);

int queue_signalled_destroy(struct queue_signalled *qs);

int queue_signalled_push(struct queue_signalled *qs, void *ptr);

int queue_signalled_pull(struct queue_signalled *qs, void **ptr);

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt);

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt);
