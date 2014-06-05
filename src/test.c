/**
 * Some basic tests
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "msg.h"
#include "utils.h"

int sd;

void quit(int sig, siginfo_t *si, void *ptr)
{
	close(sd);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
 	struct sockaddr_in sa;

	if (argc != 3) {
		printf("Usage: %s TEST IP:PORT\n", argv[0]);
		printf("  TEST has to be 'latency' for now\n");
		printf("  IP is the destination ip of our packets\n");
		printf("  PORT is the port to send to\n\n");
		printf("s2ss Simulator2Simulator Server v%s\n", VERSION);
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGINT, &sa_quit, NULL);

	/* Resolve address */
	if (resolve(argv[2], &sa, 0))
		error("Failed to resolve: %s", argv[2]);

	/* Create socket */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		error("Failed to create socket: %s", strerror(errno));

	/* Bind socket */
	if (bind(sd, (struct sockaddr *) &sa, sizeof(struct sockaddr_in)))
		error("Failed to bind socket: %s", strerror(errno));

	/* Connect socket */
	sa.sin_port = htons(ntohs(sa.sin_port) + 1);
	if (connect(sd, (struct sockaddr *) &sa, sizeof(struct sockaddr_in))) {
		error("Failed to connect socket: %s", strerror(errno));
	}


	if (!strcmp(argv[1], "latency")) {
		struct msg m2, m1 = {
			.device = 99,
			.sequence = 0
		};
		struct timespec *ts1 = (struct timespec *) &m1.data;
		struct timespec *ts2 = (struct timespec *) &m2.data;
		struct timespec *ts3 = malloc(sizeof(struct timespec));

		long long rtt, rtt_max = LLONG_MIN, rtt_min = LLONG_MAX;
		long long run = 0, avg = 0;

		while (1) {
			clock_gettime(CLOCK_REALTIME, ts1);
			send(sd, &m1, 8 + m1.length, 0);

			recv(sd, &m2, sizeof(struct msg), 0);
			clock_gettime(CLOCK_REALTIME, ts3);

			rtt = ts3->tv_nsec - ts2->tv_nsec;
			if (rtt < 0) continue;

			if (rtt > rtt_max) rtt_max = rtt;
			if (rtt < rtt_min) rtt_min = rtt;

			avg += rtt;

			printf("rtt %.3f min %.3f max %.3f avg %.3f uS\n", 1e-3 * rtt, 1e-3 * rtt_min, 1e-3 * rtt_max, 1e-3 * avg / ++run);

			m1.sequence++;
			usleep(1000);
		}
	}

	return 0;
}
