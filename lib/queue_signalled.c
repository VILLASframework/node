/** Wrapper around queue that uses POSIX CV's for signalling writes.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include "queue_signalled.h"

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memtype *mem)
{
	int ret;
	
	ret = queue_init(&qs->q, size, mem);
	if (ret < 0)
		return ret;

	pthread_mutexattr_init(&qs->mtattr);
	pthread_mutexattr_setpshared(&qs->mtattr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_init(&qs->readyattr);
	pthread_condattr_setpshared(&qs->readyattr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&qs->mt, &qs->mtattr);
	pthread_cond_init(&qs->ready, &qs->readyattr);

	return 0;
}

int queue_signalled_destroy(struct queue_signalled *qs)
{
	int ret;

	ret = queue_destroy(&qs->q);
	if (ret < 0)
		return ret;

	pthread_cond_destroy(&qs->ready);
	pthread_mutex_destroy(&qs->mt);

	return 0;
}

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
{
	int ret;
	
	ret = queue_push_many(&qs->q, ptr, cnt);
	if (ret < 0)
		return ret;
	
	pthread_mutex_lock(&qs->mt);
	pthread_cond_broadcast(&qs->ready);
	pthread_mutex_unlock(&qs->mt);

	return ret;
}

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
{
	/* Make sure that qs->mt is unlocked if this thread gets cancelled. */
	pthread_cleanup_push((void (*)(void*)) pthread_mutex_unlock, &qs->mt);
	pthread_mutex_lock(&qs->mt);
	pthread_cond_wait(&qs->ready, &qs->mt);
	pthread_mutex_unlock(&qs->mt);
	pthread_cleanup_pop(0);
	
	return queue_pull_many(&qs->q, ptr, cnt);
}
