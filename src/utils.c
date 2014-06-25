/**
 * Some helper functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "cfg.h"
#include "utils.h"

void print(enum log_level lvl, const char *fmt, ...)
{
	struct timespec ts;

	va_list ap;
	va_start(ap, fmt);

	clock_gettime(CLOCK_REALTIME, &ts);

	/* Timestamp */
	printf("%15.4f", ts.tv_sec + ts.tv_nsec / 1e9);

	switch (lvl) {
		case DEBUG: printf(" [" BLU("Debug") "] "); break;
		case INFO:  printf(" [" WHT("Info ") "] "); break;
		case WARN:  printf(" [" YEL("Warn ") "] "); break;
		case ERROR: printf(" [" RED("Error") "] "); break;
	}

	vprintf(fmt, ap);
	printf("\n");

	va_end(ap);
}

int resolve_addr(const char *addr, struct sockaddr_in *sa, int flags)
{
	int ret;

	/* Split string */
	char *tmp = strdup(addr);
	char *node = strtok(tmp, ":");
	char *service = strtok(NULL, ":");

	if (node && !strcmp(node, "*"))
		node = NULL;

	if (service && !strcmp(service, "*"))
		service = NULL;

	/* Get IP */
	struct addrinfo *result;
	struct addrinfo hint = {
		.ai_flags = flags,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = 0
	};

	ret = getaddrinfo(node, service, &hint, &result);
	if (ret)
		error("Failed to lookup address '%s': %s", addr, gai_strerror(ret));

	memcpy(sa, result->ai_addr, result->ai_addrlen);

	free(tmp);
	freeaddrinfo(result);

	return 0;
}

int sockaddr_cmp(struct sockaddr *a, struct sockaddr *b)
{
	if (a->sa_family == b->sa_family) {
		switch (a->sa_family) {
			case AF_INET: {
				struct sockaddr_in *ai = (struct sockaddr_in *) a;
				struct sockaddr_in *bi = (struct sockaddr_in *) b;

				return memcmp(&ai->sin_addr, &bi->sin_addr, sizeof(struct in_addr));
			}

			case AF_INET6: {
				struct sockaddr_in6 *ai = (struct sockaddr_in6 *) a;
				struct sockaddr_in6 *bi = (struct sockaddr_in6 *) b;

				return memcmp(&ai->sin6_addr, &bi->sin6_addr, sizeof(struct in6_addr));
			}
		}
	}

	return -1;
}

cpu_set_t to_cpu_set_t(int set)
{
	cpu_set_t cset;

	for (int i = 0; i < sizeof(int) * 8; i++) {
		if (set & (1L << i))
			CPU_SET(i, &cset);
	}

	return cset;
}
