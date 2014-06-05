/**
 * Generate random packages on stdout
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#include "config.h"
#include "utils.h"
#include "msg.h"

#define CLOCKID	CLOCK_REALTIME
#define SIG	SIGRTMIN

void tick(int sig, siginfo_t *si, void *ptr)
{
	struct msg *m = (struct msg*) si->si_value.sival_ptr;

	msg_random(m);
	msg_fprint(stdout, m);
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("Usage: %s DEVID VALUES RATE\n", argv[0]);
		printf("  DEVID  is the device id of this node\n");
		printf("  VALUES is the number of values a message contains\n");
		printf("  RATE   how many messages per second\n\n");
		printf("s2ss Simulator2Simulator Server v%s\n", VERSION);
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	int rate = atoi(argv[3]);
	struct msg msg = {
		.device = atoi(argv[1]),
		.length = atoi(argv[2]) * sizeof(double),
		.sequence = 0,
		.data = { 0 }
	};

	/* Setup signals */
	struct sigaction sa_tick = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = tick
	};

	sigemptyset(&sa_tick.sa_mask);

	if (sigaction(SIG, &sa_tick, NULL)) {
               error("Failed sigaction(): %s", strerror(errno));
	}

	/* Setup timer */
	timer_t t;
	struct sigevent sev = {
		.sigev_notify = SIGEV_SIGNAL,
		.sigev_signo = SIG,
		.sigev_value.sival_ptr = &msg
	};

	double period = 1.0 / rate;
	long sec = floor(period);
	long nsec = (period - sec) * 1000000000;

	struct itimerspec its = {
		.it_interval = { sec, nsec },
		.it_value = { 0, 1 }
	};

	/* Print header */
	fprintf(stderr, "# %-6s %-8s %-12s\n", "dev_id", "seq_no", "data");

	timer_create(CLOCKID, &sev, &t);
	timer_settime(t, 0, &its, NULL);

	while(1) pause();

	timer_delete(t);

	return 0;
}
