/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/timerfd.h>
#include <sys/syscall.h>

#include "utils.h"
#include "path.h"

#ifndef sigev_notify_thread_id
 #define sigev_notify_thread_id   _sigev_un._tid
#endif

/** Linked list of paths. */
struct list paths;

/** Send messages asynchronously */
static void * path_send(void *arg)
{
	struct path *p = arg;

	int ret;
	uint64_t runs;

	struct itimerspec its = {
		.it_interval = timespec_rate(p->rate),
		.it_value = { 1, 0 }
	};

	p->tfd = timerfd_create(CLOCK_REALTIME, 0);
	if (p->tfd < 0)
		serror("Failed to create timer");

	ret = timerfd_settime(p->tfd, 0, &its, NULL);
	if (ret)
		serror("Failed to start timer");

	while (1) {
		read(p->tfd, &runs, sizeof(runs));
		
		FOREACH(&p->destinations, it)
			node_write(it->node, p->current);

		p->sent++;
	}

	return NULL;
}

/** Receive messages */
static void * path_run(void *arg)
{
	struct path *p = arg;

	p->previous = alloc(sizeof(struct msg));
	p->current  = alloc(sizeof(struct msg));
	
	char buf[33];

	/* Open deferred TCP connection */
	node_start_defer(p->in);

	FOREACH(&p->destinations, it)
		node_start_defer(it->path->out);


	/* Main thread loop */
	while (1) {
		node_read(p->in, p->current); /* Receive message */
		
		p->received++;

		/* Check header fields */
		if (p->current->version != MSG_VERSION ||
		    p->current->type    != MSG_TYPE_DATA) {
			p->invalid++;
			continue;
		}

		/* Update histogram */
		int dist = (UINT16_MAX + p->current->sequence - p->previous->sequence) % UINT16_MAX;
		if (dist > UINT16_MAX / 2)
			dist -= UINT16_MAX;

		hist_put(&p->histogram, dist);

		/* Handle simulation restart */
		if (p->current->sequence == 0 && abs(dist) >= 1) {
			if (p->received) {
				path_print_stats(p);
				hist_print(&p->histogram);
			}

			path_print(p, buf, sizeof(buf));			
			warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u, dist=%d)",
				buf, p->previous->sequence, p->current->sequence, dist);

			/* Reset counters */
			p->sent		= 0;
			p->received	= 1;
			p->invalid	= 0;
			p->skipped	= 0;
			p->dropped	= 0;

			hist_reset(&p->histogram);
		}
		else if (dist <= 0 && p->received > 1) {
			p->dropped++;
			continue;
		}

		/* Call hook callbacks */
		FOREACH(&p->hooks, it) {
			if (it->hook(p->current, p)) {
				p->skipped++;
				continue;
			}
		}

		/* At fixed rate mode, messages are send by another thread */
		if (!p->rate) {
			FOREACH(&p->destinations, it)
				node_write(it->node, p->current);

			p->sent++;
		}

		SWAP(p->previous, p->current);
	}

	return NULL;
}

int path_start(struct path *p)
{ INDENT
	char buf[33];
	path_print(p, buf, sizeof(buf));
	
	info("Starting path: %s", buf);

	/* At fixed rate mode, we start another thread for sending */
	if (p->rate)
		pthread_create(&p->sent_tid, NULL, &path_send, p);

	return  pthread_create(&p->recv_tid, NULL, &path_run,  p);
}

int path_stop(struct path *p)
{ INDENT
	char buf[33];
	path_print(p, buf, sizeof(buf));
	
	info("Stopping path: %s", buf);

	pthread_cancel(p->recv_tid);
	pthread_join(p->recv_tid, NULL);

	if (p->rate) {
		pthread_cancel(p->sent_tid);
		pthread_join(p->sent_tid, NULL);

		close(p->tfd);
	}

	if (p->received)
		hist_print(&p->histogram);

	return 0;
}

void path_print_stats(struct path *p)
{
	char buf[33];
	path_print(p, buf, sizeof(buf));
	
	info("%-32s :   %-8u %-8u %-8u %-8u %-8u", buf,
		p->sent, p->received, p->dropped, p->skipped, p->invalid);
}

int path_print(struct path *p, char *buf, int len)
{
	*buf = 0;
	
	strap(buf, len, "%s " MAG("=>"), p->in->name);
		
	if (list_length(&p->destinations) > 1) {
		strap(buf, len, " [");
		FOREACH(&p->destinations, it)
			strap(buf, len, " %s", it->node->name);
		strap(buf, len, " ]");
	}
	else
		strap(buf, len, " %s", p->out->name);
	
	return 0;
}

struct path * path_create()
{
	struct path *p = alloc(sizeof(struct path));

	list_init(&p->destinations, NULL);
	list_init(&p->hooks, NULL);

	hist_create(&p->histogram, -HIST_SEQ, +HIST_SEQ, 1);

	return p;
}

void path_destroy(struct path *p)
{
	list_destroy(&p->destinations);
	list_destroy(&p->hooks);
	hist_destroy(&p->histogram);
	
	free(p->current);
	free(p->previous);
	free(p);
}
