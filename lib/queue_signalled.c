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

#include "queue_signalled.h"

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memtype *mem)
{
	int ret;

	pthread_condattr_t  cvattr;
	pthread_mutexattr_t mtattr;

	ret = queue_init(&qs->queue, size, mem);
	if (ret < 0)
		return ret;

	pthread_mutexattr_init(&mtattr);
	pthread_condattr_init(&cvattr);

	pthread_mutexattr_setpshared(&mtattr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_setpshared(&cvattr, PTHREAD_PROCESS_SHARED);

	pthread_mutex_init(&qs->mutex, &mtattr);
	pthread_cond_init(&qs->ready, &cvattr);

	pthread_mutexattr_destroy(&mtattr);
	pthread_condattr_destroy(&cvattr);

	return 0;
}

int queue_signalled_destroy(struct queue_signalled *qs)
{
	int ret;

	ret = queue_destroy(&qs->queue);
	if (ret < 0)
		return ret;

	pthread_cond_destroy(&qs->ready);
	pthread_mutex_destroy(&qs->mutex);

	return 0;
}

int queue_signalled_push(struct queue_signalled *qs, void *ptr)
{
	int ret;

	ret = queue_push(&qs->queue, ptr);
	if (ret < 0)
		return ret;

	pthread_mutex_lock(&qs->mutex);
	pthread_cond_broadcast(&qs->ready);
	pthread_mutex_unlock(&qs->mutex);

	return ret;
}

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
{
	int ret;

	ret = queue_push_many(&qs->queue, ptr, cnt);
	if (ret < 0)
		return ret;

	pthread_mutex_lock(&qs->mutex);
	pthread_cond_broadcast(&qs->ready);
	pthread_mutex_unlock(&qs->mutex);

	return ret;
}

int queue_signalled_pull(struct queue_signalled *qs, void **ptr)
{
	int ret = 0;
	/* Make sure that qs->mutex is unlocked if this thread gets cancelled. */
	pthread_cleanup_push((void (*)(void*)) pthread_mutex_unlock, &qs->mutex);
	pthread_mutex_lock(&qs->mutex);
	while (!ret) {
		ret = queue_pull(&qs->queue, ptr);
		if (ret == -1)
			break;
		if (!ret)
			pthread_cond_wait(&qs->ready, &qs->mutex);
	}
	pthread_mutex_unlock(&qs->mutex);
	pthread_cleanup_pop(0);
	return ret;
}

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
{
	int ret = 0;

	/* Make sure that qs->mutex is unlocked if this thread gets cancelled. */
	pthread_cleanup_push((void (*)(void*)) pthread_mutex_unlock, &qs->mutex);
	pthread_mutex_lock(&qs->mutex);
	while (!ret) {
		ret = queue_pull_many(&qs->queue, ptr, cnt);
		if (ret == -1)
			break;
		if (!ret)
			pthread_cond_wait(&qs->ready, &qs->mutex);
	}
	pthread_mutex_unlock(&qs->mutex);
	pthread_cleanup_pop(0);
	return ret;
}

int queue_signalled_close(struct queue_signalled *qs) {
	int ret;

	pthread_mutex_lock(&qs->mutex);
	ret = queue_close(&qs->queue);
	pthread_cond_broadcast(&qs->ready);
	pthread_mutex_unlock(&qs->mutex);
	return ret;
}
