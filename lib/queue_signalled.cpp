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

#include <villas/node/config.h>
#include <villas/queue_signalled.h>
#include <villas/log.h>

#ifdef HAS_EVENTFD
  #include <sys/eventfd.h>
#endif

static void queue_signalled_cleanup(void *p)
{
	struct queue_signalled *qs = (struct queue_signalled *) p;

	if (qs->mode == QueueSignalledMode::PTHREAD)
		pthread_mutex_unlock(&qs->pthread.mutex);
}

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memory_type *mem, enum QueueSignalledMode mode, int flags)
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
#elif defined(__APPLE__)
		if (flags & (int) QueueSignalledFlags::PROCESS_SHARED)
			qs->mode = QueueSignalledMode::PTHREAD;
		else
			qs->mode = QueueSignalledMode::PIPE;
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

		pthread_mutexattr_init(&mtattr);
		pthread_condattr_init(&cvattr);

		if (flags & (int) QueueSignalledFlags::PROCESS_SHARED) {
			pthread_mutexattr_setpshared(&mtattr, PTHREAD_PROCESS_SHARED);
			pthread_condattr_setpshared(&cvattr, PTHREAD_PROCESS_SHARED);
		}

		pthread_mutex_init(&qs->pthread.mutex, &mtattr);
		pthread_cond_init(&qs->pthread.ready, &cvattr);

		pthread_mutexattr_destroy(&mtattr);
		pthread_condattr_destroy(&cvattr);
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
#elif defined(__APPLE__)
	else if (qs->mode == QueueSignalledMode::PIPE) {
		ret = pipe(qs->pipe);
		if (ret < 0)
			return -2;
	}
#endif
	else
		return -1;

	return 0;
}

int queue_signalled_destroy(struct queue_signalled *qs)
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
#elif defined(__APPLE__)
	else if (qs->mode == QueueSignalledMode::PIPE) {
		ret = close(qs->pipe[0]) + close(qs->pipe[1]);
		if (ret)
			return ret;
	}
#endif
	else
		return -1;

	return 0;
}

int queue_signalled_push(struct queue_signalled *qs, void *ptr)
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
#elif defined(__APPLE__)
	else if (qs->mode == QueueSignalledMode::PIPE) {
		int ret;
		uint8_t incr = 1;
		ret = write(qs->pipe[1], &incr, sizeof(incr));
		if (ret < 0)
			return ret;
	}
#endif
	else
		return -1;

	return pushed;
}

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
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
#elif defined(__APPLE__)
	else if (qs->mode == QueueSignalledMode::PIPE) {
		int ret;
		uint8_t incr = 1;
		ret = write(qs->pipe[1], &incr, sizeof(incr));
		if (ret < 0)
			return ret;
	}
#endif
	else
		return -1;

	return pushed;
}

int queue_signalled_pull(struct queue_signalled *qs, void **ptr)
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
#elif defined(__APPLE__)
			else if (qs->mode == QueueSignalledMode::PIPE) {
				int ret;
				uint8_t incr = 1;
				ret = read(qs->pipe[0], &incr, sizeof(incr));
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

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
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
#elif defined(__APPLE__)
			else if (qs->mode == QueueSignalledMode::PIPE) {
				int ret;
				uint8_t incr = 1;
				ret = read(qs->pipe[0], &incr, sizeof(incr));
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

int queue_signalled_close(struct queue_signalled *qs)
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
#elif defined(__APPLE__)
	else if (qs->mode == QueueSignalledMode::PIPE) {
		int ret;
		uint64_t incr = 1;

		ret = write(qs->pipe[1], &incr, sizeof(incr));
		if (ret < 0)
			return ret;
	}
#endif
	else
		return -1;

	return ret;
}

int queue_signalled_fd(struct queue_signalled *qs)
{
	switch (qs->mode) {
#ifdef HAS_EVENTFD
		case QueueSignalledMode::EVENTFD:
			return qs->eventfd;
#elif defined(__APPLE__)
		case QueueSignalledMode::PIPE:
			return qs->pipe[0];
#endif
		default: { }
	}

	return -1;
}
