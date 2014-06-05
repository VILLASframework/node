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
 	struct sockaddr_in sa;

	if (argc != 3 && argc != 4) {
		printf("Usage: %s VALUES REMOTE [LOCAL]\n", argv[0]);
		printf("  REMOTE   is a IP:PORT combination of the remote host\n");
		printf("  LOCAL    is an optional IP:PORT combination of the local host\n");
		printf("  VALUES   is the number of values to be read from stdin\n\n");
		printf("s2ss Simulator2Simulator Server v%s\n", VERSION);
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	int values = atoi(argv[1]);
	const char *remote_str = argv[2];
	const char *local_str = argv[3];

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

	if (argc == 4 && resolve_addr(remote_str, &remote, 0))
		error("Failed to resolve local address: %s", local_str);
	else {
		local.sin_family = AF_INET;
		local.sin_addr.s_addr = INADDR_ANY;
		local.sin_port = 0;
	}

	if (resolve_addr(remote_str, &remote, 0))
		error("Failed to resolve remote address: %s", remote_str);

	/* Create node */
	struct node n;
	node_create(&n, NULL, NODE_SERVER, local, remote);
	node_connect(&n);

	struct msg m;
	m.length = values * sizeof(double);

	while (!feof(stdin)) {
		msg_fscan(stdin, &m);
		msg_send(&m, &n);
		msg_fprint(stdout, &m);
	}

	return 0;
}
