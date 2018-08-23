/** Unit tests for queue_signalled
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <pthread.h>
#include <poll.h>

#include <villas/utils.h>
#include <villas/memory.h>
#include <villas/queue_signalled.h>

extern void init_memory();

#define NUM_ELEM 1000

struct param {
	int flags;
	bool polled;
};

static void * producer(void * ctx)
{
	int ret;
	struct queue_signalled *q = (struct queue_signalled *) ctx;

	for (intptr_t i = 0; i < NUM_ELEM; i++) {
		ret = queue_signalled_push(q, (void *) i);
		if (ret != 1)
			return (void *) 1; /* Indicates an error to the parent thread */

		usleep(0.1e-3 * 1e6); /* 1 ms */
	}

	return NULL;
}

static void * consumer(void * ctx)
{
	int ret;
	struct queue_signalled *q = (struct queue_signalled *) ctx;

	void *data[NUM_ELEM];

	for (intptr_t i = 0; i < NUM_ELEM;) {
		ret = queue_signalled_pull_many(q, data, ARRAY_LEN(data));
		if (ret <= 0)
			return (void *) 1; /* Indicates an error to the parent thread */

		for (intptr_t j = 0; j < ret; j++, i++) {
			if ((intptr_t) data[j] != i)
				return (void *) 2; /* Indicates an error to the parent thread */
		}
	}

	return NULL;
}

 void * polled_consumer(void *ctx)
{
	int ret, fd;
	struct queue_signalled *q = (struct queue_signalled *) ctx;

	fd = queue_signalled_fd(q);
	cr_assert_geq(fd, 0);

	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN
	};

	for (intptr_t i = 0; i < NUM_ELEM; i++) {
again:		ret = poll(&pfd, 1, -1);
		if (ret < 0)
			return (void *) 3;
		else if (ret == 0)
			goto again;


		void *p;
		ret = queue_signalled_pull(q, &p);
		if (ret != 1)
			return (void *) 1; /* Indicates an error to the parent thread */

		if ((intptr_t) p != i)
			return (void *) 2; /* Indicates an error to the parent thread */
	}

	return NULL;
}

ParameterizedTestParameters(queue_signalled, simple)
{
	static struct param params[] = {
		{ 0, false },
		{ QUEUE_SIGNALLED_PTHREAD, false },
		{ QUEUE_SIGNALLED_PTHREAD, false },
		{ QUEUE_SIGNALLED_PTHREAD | QUEUE_SIGNALLED_PROCESS_SHARED, false },
		{ QUEUE_SIGNALLED_POLLING, false },
#if defined(__linux__) && defined(HAS_EVENTFD)
		{ QUEUE_SIGNALLED_EVENTFD, false },
		{ QUEUE_SIGNALLED_EVENTFD, true }
#endif
	};

	return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

ParameterizedTest(struct param *param, queue_signalled, simple, .timeout = 5, .init = init_memory)
{
	int ret;
	void *r1, *r2;
	struct queue_signalled q;
	q.queue.state = STATE_DESTROYED;

	pthread_t t1, t2;

	ret = queue_signalled_init(&q, LOG2_CEIL(NUM_ELEM), &memory_heap, param->flags);
	cr_assert_eq(ret, 0, "Failed to initialize queue: flags=%#x, ret=%d", param->flags, ret);

	ret = pthread_create(&t1, NULL, producer, &q);
	cr_assert_eq(ret, 0);

	ret = pthread_create(&t2, NULL, param->polled ? polled_consumer : consumer, &q);
	cr_assert_eq(ret, 0);

	ret = pthread_join(t1, &r1);
	cr_assert_eq(ret, 0);

	ret = pthread_join(t2, &r2);
	cr_assert_eq(ret, 0);

	cr_assert_null(r1, "Producer failed: %p", r1);
	cr_assert_null(r2, "Consumer failed: %p", r2);

	ret = queue_signalled_available(&q);
	cr_assert_eq(ret, 0);

	ret = queue_signalled_close(&q);
	cr_assert_eq(ret, 0);

	ret = queue_signalled_destroy(&q);
	cr_assert_eq(ret, 0);
}
