/**
 * Receive messages from server snd print them on stdout
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
	struct node n;
	struct msg m;

	memset(&n, 0, sizeof(struct node));
 	struct sockaddr_in sa;

	if (argc != 2) {
		printf("Usage: %s LOCAL\n", argv[0]);
		printf("  LOCAL   is a IP:PORT combination of the local host\n\n");
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
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);

	/* Resolve addresses */
	if (resolve_addr(argv[1], &n.local, 0))
		error("Failed to resolve local address: %s", argv[1]);

	/* Print header */
	fprintf(stderr, "# %-6s %-8s %-12s\n", "dev_id", "seq_no", "data");

	node_connect(&n);

	while (1) {
		msg_recv(&m, &n);
		msg_fprint(stdout, &m);
	}

	return 0;
}
