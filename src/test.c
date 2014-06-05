/**
 * Simple UDP test client (simulator emulation)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "utils.h"
#include "msg.h"

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("Usage: test DEVID IP PORT\n");
		printf("  DEVID is the device id of this node\n");
		printf("  IP is the destination ip of our packets\n");
		printf("  PORT is the port to recv/send from\n\n");
		printf("s2ss Simulator2Simulator Server v%s\n", VERSION);
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	int dev_id = atoi(argv[1]);
	int ret;

	print(INFO, "Test node started on %s:%s with id=%u", argv[2], argv[3], dev_id);

	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		print(FATAL, "Failed to create socket: %s", strerror(errno));

	struct sockaddr_in sa;
	struct msg m;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(atoi(argv[3]));
	inet_aton(argv[2], &sa.sin_addr);

	ret = connect(sd, &sa, sizeof(struct sockaddr_in));
	if (ret < 0)
		print(FATAL, "Failed to connect socket: %s", strerror(errno));

	while (1) {
		msg_random(&m, dev_id);
		msg_fprint(stdout, &m);

		send(sd, &m, sizeof(struct msg), 0);

		sleep(3);
	}

	pause();
	return 0;
}
