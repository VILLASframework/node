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
#include <arpa/inet.h>

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
	if (argc != 4) {
		printf("Usage: %s TEST LOCAL REMOTE\n", argv[0]);
		printf("  TEST     has to be 'latency' for now\n");
		printf("  LOCAL    is a IP:PORT combination of the local host\n");
		printf("  REMOTE   is a IP:PORT combination of the remote host\n\n");
		printf("s2ss Simulator2Simulator Server v%s\n", VERSION);
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	const char *test = argv[1];
	const char *local_str = argv[2];
	const char *remote_str = argv[3];

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);

	/* Resolve addresses */
	struct sockaddr_in local;
 	struct sockaddr_in remote;

	if (resolve_addr(local_str, &local, 0))
		error("Failed to resolve local address: %s", local_str);

	if (resolve_addr(remote_str, &remote, 0))
		error("Failed to resolve remote address: %s", remote_str);

	/* Create node */
	struct node n;
	node_create(&n, NULL, NODE_SERVER, local, remote);
	node_connect(&n);

	debug(1, "We listen at %s:%u", inet_ntoa(n.local.sin_addr), ntohs(n.local.sin_port));
	debug(1, "We sent to %s:%u", inet_ntoa(n.remote.sin_addr), ntohs(n.remote.sin_port));

	if (!strcmp(test, "latency")) {
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
			msg_send(&m1, &n);
			msg_recv(&m2, &n);
			clock_gettime(CLOCK_REALTIME, ts3);

			rtt = ts3->tv_nsec - ts2->tv_nsec;
			if (rtt < 0) continue;

			if (rtt > rtt_max) rtt_max = rtt;
			if (rtt < rtt_min) rtt_min = rtt;

			avg += rtt;

			info("rtt %.3f min %.3f max %.3f avg %.3f uS\n", 1e-3 * rtt, 1e-3 * rtt_min, 1e-3 * rtt_max, 1e-3 * avg / ++run);

			m1.sequence++;
			usleep(1000);
		}
	}

	return 0;
}
