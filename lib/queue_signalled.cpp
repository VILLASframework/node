/** Wrapper around queue that uses POSIX CV's for signalling writes.
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

#include <villas/node/config.hpp>
#include <villas/queue_signalled.h>

#ifdef HAS_EVENTFD
  #include <sys/eventfd.h>
#endif

using namespace villas::node;

static
void queue_signalled_cleanup(void *p)
{
	struct CQueueSignalled *qs = (struct CQueueSignalled *) p;

	if (qs->mode == QueueSignalledMode::PTHREAD)
		pthread_mutex_unlock(&qs->pthread.mutex);
}

int villas::node::queue_signalled_init(struct CQueueSignalled *qs, size_t size, struct memory::Type *mem, enum QueueSignalledMode mode, int flags)
{
	int ret;

	qs->mode = mode;

	if (qs->mode == QueueSignalledMode::AUTO) {
#ifdef __linux__
		if (flags & (int) QueueSignalledFlags::PROCESS_SHARED)
			qs->mode = QueueSignalledMode::PTHREAD;
		else {
#ifdef HAS_EVENTFD
			qs->mode = QueueSignalledMode::EVENTFD;
#else
			qs->mode = QueueSignalledMode::PTHREAD;
#endif
		}
#else
		qs->mode = QueueSignalledMode::PTHREAD;
#endif
	}

	ret = queue_init(&qs->queue, size, mem);
	if (ret < 0)
		return ret;

	if (qs->mode == QueueSignalledMode::PTHREAD) {
		pthread_condattr_t  cvattr;
		pthread_mutexattr_t mtattr;

		ret = pthread_mutexattr_init(&mtattr);
		if (ret)
			return ret;

		ret = pthread_condattr_init(&cvattr);
		if (ret)
			return ret;

		if (flags & (int) QueueSignalledFlags::PROCESS_SHARED) {
			ret = pthread_mutexattr_setpshared(&mtattr, PTHREAD_PROCESS_SHARED);
			if (ret)
				return ret;

			ret = pthread_condattr_setpshared(&cvattr, PTHREAD_PROCESS_SHARED);
			if (ret)
				return ret;
		}

		ret = pthread_mutex_init(&qs->pthread.mutex, &mtattr);
		if (ret)
			return ret;

		ret = pthread_cond_init(&qs->pthread.ready, &cvattr);
		if (ret)
			return ret;

		ret = pthread_mutexattr_destroy(&mtattr);
		if (ret)
			return ret;

		ret = pthread_condattr_destroy(&cvattr);
		if (ret)
			return ret;
	}
	else if (qs->mode == QueueSignalledMode::POLLING) {
		/* Nothing todo */
	}
#ifdef HAS_EVENTFD
	else if (qs->mode == QueueSignalledMode::EVENTFD) {
		qs->eventfd = eventfd(0, 0);
		if (qs->eventfd < 0)
			return -2;
	}
#endif
	else
		return -1;

	return 0;
}

int villas::node::queue_signalled_destroy(struct CQueueSignalled *qs)
{
	int ret;

	ret = queue_destroy(&qs->queue);
	if (ret < 0)
		return ret;

	if (qs->mode == QueueSignalledMode::PTHREAD) {
		pthread_cond_destroy(&qs->pthread.ready);
		pthread_mutex_destroy(&qs->pthread.mutex);
	}
	else if (qs->mode == QueueSignalledMode::POLLING) {
		/* Nothing todo */
	}
#ifdef HAS_EVENTFD
	else if (qs->mode == QueueSignalledMode::EVENTFD) {
		ret = close(qs->eventfd);
		if (ret)
			return ret;
	}
#endif
	else
		return -1;

	return 0;
}

int villas::node::queue_signalled_push(struct CQueueSignalled *qs, void *ptr)
{
	int pushed;

	pushed = queue_push(&qs->queue, ptr);
	if (pushed < 0)
		return pushed;

	if (qs->mode == QueueSignalledMode::PTHREAD) {
		pthread_mutex_lock(&qs->pthread.mutex);
		pthread_cond_broadcast(&qs->pthread.ready);
		pthread_mutex_unlock(&qs->pthread.mutex);
	}
	else if (qs->mode == QueueSignalledMode::POLLING) {
		/* Nothing todo */
	}
#ifdef HAS_EVENTFD
	else if (qs->mode == QueueSignalledMode::EVENTFD) {
		int ret;
		uint64_t incr = 1;
		ret = write(qs->eventfd, &incr, sizeof(incr));
		if (ret < 0)
			return ret;
	}
#endif
	else
		return -1;

	return pushed;
}

int villas::node::queue_signalled_push_many(struct CQueueSignalled *qs, void *ptr[], size_t cnt)
{
	int pushed;

	pushed = queue_push_many(&qs->queue, ptr, cnt);
	if (pushed < 0)
		return pushed;

	if (qs->mode == QueueSignalledMode::PTHREAD) {
		pthread_mutex_lock(&qs->pthread.mutex);
		pthread_cond_broadcast(&qs->pthread.ready);
		pthread_mutex_unlock(&qs->pthread.mutex);
	}
	else if (qs->mode == QueueSignalledMode::POLLING) {
		/* Nothing todo */
	}
#ifdef HAS_EVENTFD
	else if (qs->mode == QueueSignalledMode::EVENTFD) {
		int ret;
		uint64_t incr = 1;
		ret = write(qs->eventfd, &incr, sizeof(incr));
		if (ret < 0)
			return ret;
	}
#endif
	else
		return -1;

	return pushed;
}

int villas::node::queue_signalled_pull(struct CQueueSignalled *qs, void **ptr)
{
	int pulled = 0;

	/* Make sure that qs->mutex is unlocked if this thread gets cancelled. */
	pthread_cleanup_push(queue_signalled_cleanup, qs);

	if (qs->mode == QueueSignalledMode::PTHREAD)
		pthread_mutex_lock(&qs->pthread.mutex);

	while (!pulled) {
		pulled = queue_pull(&qs->queue, ptr);
		if (pulled < 0)
			break;
		else if (pulled == 0) {
			if (qs->mode == QueueSignalledMode::PTHREAD)
				pthread_cond_wait(&qs->pthread.ready, &qs->pthread.mutex);
			else if (qs->mode == QueueSignalledMode::POLLING)
				continue; /* Try again */
#ifdef HAS_EVENTFD
			else if (qs->mode == QueueSignalledMode::EVENTFD) {
				int ret;
				uint64_t cntr;
				ret = read(qs->eventfd, &cntr, sizeof(cntr));
				if (ret < 0)
					break;
			}
#endif
			else
				break;
		}
	}

	if (qs->mode == QueueSignalledMode::PTHREAD)
		pthread_mutex_unlock(&qs->pthread.mutex);

	pthread_cleanup_pop(0);

	return pulled;
}

int villas::node::queue_signalled_pull_many(struct CQueueSignalled *qs, void *ptr[], size_t cnt)
{
	int pulled = 0;

	/* Make sure that qs->mutex is unlocked if this thread gets cancelled. */
	pthread_cleanup_push(queue_signalled_cleanup, qs);

	if (qs->mode == QueueSignalledMode::PTHREAD)
		pthread_mutex_lock(&qs->pthread.mutex);

	while (!pulled) {
		pulled = queue_pull_many(&qs->queue, ptr, cnt);
		if (pulled < 0)
			break;
		else if (pulled == 0) {
			if (qs->mode == QueueSignalledMode::PTHREAD)
				pthread_cond_wait(&qs->pthread.ready, &qs->pthread.mutex);
			else if (qs->mode == QueueSignalledMode::POLLING)
				continue; /* Try again */
#ifdef HAS_EVENTFD
			else if (qs->mode == QueueSignalledMode::EVENTFD) {
				int ret;
				uint64_t cntr;
				ret = read(qs->eventfd, &cntr, sizeof(cntr));
				if (ret < 0)
					break;
			}
#endif
			else
				break;
		}
	}

	if (qs->mode == QueueSignalledMode::PTHREAD)
		pthread_mutex_unlock(&qs->pthread.mutex);

	pthread_cleanup_pop(0);

	return pulled;
}

int villas::node::queue_signalled_close(struct CQueueSignalled *qs)
{
	int ret;

	if (qs->mode == QueueSignalledMode::PTHREAD)
		pthread_mutex_lock(&qs->pthread.mutex);

	ret = queue_close(&qs->queue);

	if (qs->mode == QueueSignalledMode::PTHREAD) {
		pthread_cond_broadcast(&qs->pthread.ready);
		pthread_mutex_unlock(&qs->pthread.mutex);
	}
	else if (qs->mode == QueueSignalledMode::POLLING) {
		/* Nothing todo */
	}
#ifdef HAS_EVENTFD
	else if (qs->mode == QueueSignalledMode::EVENTFD) {
		int ret;
		uint64_t incr = 1;

		ret = write(qs->eventfd, &incr, sizeof(incr));
		if (ret < 0)
			return ret;
	}
#endif
	else
		return -1;

	return ret;
}

int villas::node::queue_signalled_fd(struct CQueueSignalled *qs)
{
	switch (qs->mode) {
#ifdef HAS_EVENTFD
		case QueueSignalledMode::EVENTFD:
			return qs->eventfd;
#endif
		default: { }
	}

	return -1;
}
