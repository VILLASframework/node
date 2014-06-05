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
 	struct sockaddr_in sa;

	if (argc != 2) {
		printf("Usage: %s IP:PORT\n", argv[0]);
		printf("  IP is the destination ip of our packets\n");
		printf("  PORT is the port to receive from\n\n");
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

	/* Resolve address */
	if (resolve(argv[1], &sa, 0))
		error("Failed to resolve: %s", argv[1]);

	/* Create socket */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		perror("Failed to create socket");

	/* Bind socket */
	if (bind(sd, (struct sockaddr *) &sa, sizeof(struct sockaddr_in)))
		perror("Failed to bind to socket");

	struct msg m;
	while (1) {
		recv(sd, &m, sizeof(struct msg), 0);
		msg_fprint(stdout, &m);
	}

	return 0;
}
