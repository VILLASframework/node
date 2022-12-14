/** Lock-free Multiple-Producer Multiple-consumer (MPMC) queue.
 *
 * Based on Dmitry Vyukov#s Bounded MPMC queue:
 *   http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2021, Steffen Vogel
 * @license BSD 2-Clause License
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modiffication, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************************/

#pragma once

#include <atomic>

#include <cstddef>
#include <cstdint>
#include <unistd.h>

#include <villas/common.hpp>
#include <villas/node/config.hpp>
#include <villas/node/memory_type.hpp>

namespace villas {
namespace node {

typedef char cacheline_pad_t[CACHELINE_SIZE];

struct CQueue_cell {
	std::atomic<size_t> sequence;
	off_t data_off; /**< Pointer relative to the queue struct */
};

/** A lock-free multiple-producer, multiple-consumer (MPMC) queue. */
struct CQueue {
	std::atomic<enum State> state;

	cacheline_pad_t _pad0;	/**< Shared area: all threads read */

	size_t buffer_mask;
	off_t buffer_off;	/**< Relative pointer to struct CQueue_cell[] */

	cacheline_pad_t	_pad1;	/**< Producer area: only producers read & write */

	std::atomic<size_t>	tail;	/**< Queue tail pointer */

	cacheline_pad_t	_pad2;	/**< Consumer area: only consumers read & write */

	std::atomic<size_t>	head;	/**< Queue head pointer */

	cacheline_pad_t	_pad3;	/**< @todo Why needed? */
};

/** Initialize MPMC queue */
int queue_init(struct CQueue *q, size_t size, struct memory::Type *mem = memory::default_type) __attribute__ ((warn_unused_result));

/** Desroy MPMC queue and release memory */
int queue_destroy(struct CQueue *q) __attribute__ ((warn_unused_result));

/** Return estimation of current queue usage.
 *
 * Note: This is only an estimation and not accurate as long other
 *       threads are performing operations.
 */
size_t queue_available(struct CQueue *q);

int queue_push(struct CQueue *q, void *ptr);

int queue_pull(struct CQueue *q, void **ptr);

/** Enqueue up to \p cnt pointers of the \p ptr array into the queue.
 *
 * @return The number of pointers actually enqueued.
 *         This number can be smaller then \p cnt in case the queue is filled.
 */
int queue_push_many(struct CQueue *q, void *ptr[], size_t cnt);

/** Dequeue up to \p cnt pointers from the queue and place them into the \p ptr array.
 *
 * @return The number of pointers actually dequeued.
 *         This number can be smaller than \p cnt in case the queue contained less than
 *         \p cnt elements.
 */
int queue_pull_many(struct CQueue *q, void *ptr[], size_t cnt);

/** Closes the queue, causing following writes to fail and following reads (after
 * the queue is empty) to fail.
 *
 * @return 0 on success.
 * @return -1 on failure.
 */
int queue_close(struct CQueue *q);

} /* namespace node */
} /* namespace villas */
