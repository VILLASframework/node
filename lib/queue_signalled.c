/** Wrapper around queue that uses POSIX CV's for signalling writes. */

#include "queue_signalled.h"

int queue_signalled_init(struct queue_signalled *qs, size_t size, struct memtype *mem)
{
	int r = queue_init(&qs->q, size, mem);
	if (r < 0)
		return r;
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
	int r = queue_destroy(&qs->q);
	if (r < 0)
		return r;
	pthread_cond_destroy(&qs->ready);
	pthread_mutex_destroy(&qs->mt);
	return 0;
}

int queue_signalled_push_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
{
	int r = queue_push_many(&qs->q, ptr, cnt);
	if (r > 0) {
		pthread_mutex_lock(&qs->mt);
		pthread_cond_broadcast(&qs->ready);
		pthread_mutex_unlock(&qs->mt);
	}
	return r;
}

int queue_signalled_pull_many(struct queue_signalled *qs, void *ptr[], size_t cnt)
{
	pthread_mutex_lock(&qs->mt);
	pthread_cond_wait(&qs->ready, &qs->mt);
	pthread_mutex_unlock(&qs->mt);
	return queue_pull_many(&qs->q, ptr, cnt);
}
