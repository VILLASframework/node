/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

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

	int sig;
	sigset_t set;

	struct sigevent sev = {
		.sigev_notify = SIGEV_THREAD_ID,
		.sigev_signo = SIGALRM,
		.sigev_notify_thread_id = syscall(SYS_gettid)
	};

	struct itimerspec its = {
		.it_interval = timespec_rate(p->rate),
		.it_value = { 1, 0 }
	};

	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	if(pthread_sigmask(SIG_BLOCK, &set, NULL))
		serror("Set signal mask");

	if (timer_create(CLOCK_REALTIME, &sev, &p->timer))
		serror("Failed to create timer");

	if (timer_settime(p->timer, 0, &its, NULL))
		serror("Failed to start timer");

	while (1) {
		sigwait(&set, &sig); /* blocking wait for next timer tick */
		
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
	// FIXME: node_start_defer(p->out);


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

		timer_delete(p->timer);
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
	
	if (list_length(&p->destinations) > 1) {
		strap(buf, len, "%s " MAG("=>") " [", p->in->name);
		FOREACH(&p->destinations, it)
			strap(buf, len, " %s", it->node->name);
		strap(buf, len, " ]");
	}
	else
		strap(buf, len, "%s " MAG("=>") " %s", p->in->name, p->out->name);
	
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
