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

#define sigev_notify_thread_id   _sigev_un._tid

/** Linked list of paths */
struct path *paths;

/** Send messages */
static void * path_send(void *arg)
{
	int sig;
	struct path *p = (struct path *) arg;
	timer_t tmr;
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

	if (timer_create(CLOCK_REALTIME, &sev, &tmr))
		serror("Failed to create timer");

	if (timer_settime(tmr, 0, &its, NULL))
		serror("Failed to start timer");

	while (1) {
		sigwait(&set, &sig); /* blocking wait for next timer tick */
		if (p->last) {
			node_write(p->out, p->last);
			p->last = NULL;
			p->sent++;
		}
	}

	return NULL;
}

/** Receive messages */
static void * path_run(void *arg)
{
	struct path *p = (struct path *) arg;
	struct msg  *m = malloc(sizeof(struct msg));
	if (!m)
		error("Failed to allocate memory for message!");
	
	/* Open deferred TCP connection */
	node_start_defer(p->in);
	node_start_defer(p->out);

	/* Main thread loop */
	while (1) {
		node_read(p->in, m); /* Receive message */
		
		p->received++;

		/* Check header fields */
		if (m->version != MSG_VERSION ||
		    m->type    != MSG_TYPE_DATA) {
			p->invalid++;
			continue;
		}

		/* Update histogram */
		int dist = (UINT16_MAX + m->sequence - p->sequence) % UINT16_MAX;
		if (dist > UINT16_MAX / 2)
			dist -= UINT16_MAX;

		int idx = HIST_SEQ / 2 + dist;
		if (idx < HIST_SEQ && idx >= 0)
			p->histogram[idx]++;

		/* Handle simulation restart */
		if (m->sequence == 0 && abs(dist) > 16) {
			path_stats(p);
			warn("Simulation for path %s " MAG("=>") " %s "
			     "restarted (p->seq=%u, m->seq=%u, dist=%d)",
			     	p->in->name, p->out->name,
				p->sequence, m->sequence, dist);

			/* Reset counters */
			p->sent		= 0;
			p->received	= 1;
			p->invalid	= 0;
			p->skipped	= 0;
			p->dropped	= 0;

			/* Reset sequence no tracking */
			memset(p->histogram, 0, sizeof(p->histogram));
		}
		else if (dist <= 0 && p->received > 1) {
			p->dropped++;
			continue;
		}

		/* Call hook callbacks */
		if (p->hook && p->hook(m, p)) {
			p->skipped++;
			continue;
		}

		/* Update last known sequence number */
		p->sequence = m->sequence;
		p->last = m;

		/* At fixed rate mode, messages are send by another thread */
		if (!p->rate) {
			node_write(p->out, m); /* Send message */
			p->sent++;
		}
	}

	free(m);

	return NULL;
}

int path_start(struct path *p)
{ INDENT
	info("Starting path: %12s " GRN("=>") " %-12s", p->in->name, p->out->name);

	/* At fixed rate mode, we start another thread for sending */
	if (p->rate)
		pthread_create(&p->sent_tid, NULL, &path_send, (void *) p);

	return  pthread_create(&p->recv_tid, NULL, &path_run,  (void *) p);
}

int path_stop(struct path *p)
{ INDENT
	info("Stopping path: %12s " RED("=>") " %-12s", p->in->name, p->out->name);

	pthread_cancel(p->recv_tid);
	pthread_join(p->recv_tid, NULL);

	if (p->rate) {
		pthread_cancel(p->sent_tid);
		pthread_join(p->sent_tid, NULL);
	}

	if (p->received) {
		path_stats(p);
		hist_plot(p->histogram, HIST_SEQ);
		hist_dump(p->histogram, HIST_SEQ);
	}

	return 0;
}

void path_stats(struct path *p)
{
	info("%12s " MAG("=>") " %-12s:   %-8u %-8u %-8u %-8u %-8u",
		p->in->name, p->out->name,
		p->sent, p->received, p->dropped, p->skipped, p->invalid
	);
}
