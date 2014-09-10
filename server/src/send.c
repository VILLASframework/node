/** Send messages from stdin to server.
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
#include <netdb.h>

#include "config.h"
#include "utils.h"
#include "node.h"
#include "msg.h"

int sd;

void quit(int sig, siginfo_t *si, void *ptr)
{
	close(sd);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	if (argc != 2 && argc != 3) {
		printf("Usage: %s REMOTE [LOCAL]\n", argv[0]);
		printf("  REMOTE   is a IP:PORT combination of the remote host\n");
		printf("  LOCAL    is an optional IP:PORT combination of the local host\n");
		printf("Simulator2Simulator Server %s (built on %s %s)\n",
			BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	struct node n = NODE_INIT("remote");
	struct msg  m = MSG_INIT(0);

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);

	/* Resolve addresses */
	int ret = resolve_addr(argv[1], &n.remote, 0);
	if (ret)
		error("Failed to resolve remote address '%s': %s", argv[1], gai_strerror(ret));

	if (argc == 3) {
		ret = resolve_addr(argv[2], &n.local, AI_PASSIVE);
		if (ret)
			error("Failed to resolve local address '%s': %s", argv[2], gai_strerror(ret));
	}
	else {
		n.local.sin_family = AF_INET;
		n.local.sin_addr.s_addr = INADDR_ANY; /* all local interfaces */
		n.local.sin_port = 0; /* random port */
	}

	node_connect(&n);

	while (!feof(stdin)) {
		msg_fscan(stdin, &m);

#if 1 /* Preprend timestamp */
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		fprintf(stdout, "%17.3f\t", ts.tv_sec + ts.tv_nsec / 1e9);
#endif

		msg_fprint(stdout, &m);
		msg_send(&m, &n);	
	}

	return 0;
}
