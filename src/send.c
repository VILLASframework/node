/**
 * Send messages from stdin to server
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "utils.h"
#include "msg.h"

int sd;

void quit(int sig, siginfo_t *si, void *ptr)
{
	close(sd);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	if (argc != 3 && argc != 4) {
		printf("Usage: %s VALUES REMOTE [LOCAL]\n", argv[0]);
		printf("  REMOTE   is a IP:PORT combination of the remote host\n");
		printf("  LOCAL    is an optional IP:PORT combination of the local host\n");
		printf("  VALUES   is the number of values to be read from stdin\n\n");
		printf("s2ss Simulator2Simulator Server v%s\n", VERSION);
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	struct node n;
	struct msg m = {
		.length = atoi(argv[1]) * sizeof(double)
	};

	memset(&n, 0, sizeof(struct node));

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);

	/* Resolve addresses */
	if (resolve_addr(argv[2], &n.remote, 0))
		error("Failed to resolve remote address: %s", argv[2]);

	if (argc == 4 && resolve_addr(argv[3], &n.local, 0))
		error("Failed to resolve local address: %s", argv[3]);
	else {
		n.local.sin_family = AF_INET;
		n.local.sin_addr.s_addr = INADDR_ANY;
		n.local.sin_port = 0;
	}

	node_connect(&n);

	while (!feof(stdin)) {
		msg_fscan(stdin, &m);
		msg_send(&m, &n);

#if 1
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		fprintf(stdout, "%17.6f", ts.tv_sec + ts.tv_nsec / 1e9);
#endif

		msg_fprint(stdout, &m);
	}

	return 0;
}
