/** Interface related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include "if.h"
#include "tc.h"
#include "socket.h"
#include "utils.h"

/** Linked list of interfaces. */
struct list interfaces;

struct interface * if_create(int index) {
	struct interface *i = alloc(sizeof(struct interface));

	i->index = index;
	if_indextoname(index, i->name);

	debug(3, "Created interface '%s' (index=%u)", i->name, i->index);

	list_init(&i->sockets, NULL);
	list_push(&interfaces, i);

	return i;
}

void if_destroy(struct interface *i)
{
	/* List members are freed by their belonging nodes. */
	list_destroy(&i->sockets);

	free(i);
}

int if_start(struct interface *i, int affinity)
{
	if (!i->refcnt) {
		warn("Interface '%s' is not used by an active node", i->name);
		return -1;
	}
	else
		info("Starting interface '%s' (index=%u)", i->name, i->index);
	
	{ INDENT
		/* Assign fwmark's to socket nodes which have netem options */
		int mark = 0;
		FOREACH(&i->sockets, it) {
			struct socket *s = it->socket;
			if (s->netem)
				s->mark = 1 + mark++;
		}
		
		/* Abort if no node is using netem */
		if (mark == 0)
			return 0;

		/* Replace root qdisc */
		tc_prio(i, TC_HDL(4000, 0), mark);
		
		/* Create netem qdisks and appropriate filter per netem node */
		FOREACH(&i->sockets, it) {
			struct socket *s = it->socket;
			if (s->netem) {
				tc_mark(i,  TC_HDL(4000, s->mark), s->mark);
				tc_netem(i, TC_HDL(4000, s->mark), s->netem);
			}
		}

		/* Set affinity for network interfaces (skip _loopback_ dev) */
		if_getirqs(i);
		if_setaffinity(i, affinity);
	}

	return 0;
}

int if_stop(struct interface *i)
{
	info("Stopping interface '%s' (index=%u)", i->name, i->index);

	{ INDENT
		if_setaffinity(i, -1L);
		tc_reset(i);
	}

	return 0;
}

int if_getegress(struct sockaddr *sa)
{
	switch (sa->sa_family) {
		case AF_INET: {
			struct sockaddr_in *sin = (struct sockaddr_in *) sa;
			char cmd[128];
			char token[32];

			snprintf(cmd, sizeof(cmd), "ip route get %s", inet_ntoa(sin->sin_addr));

			debug(8, "System: %s", cmd);

			FILE *p = popen(cmd, "r");
			if (!p)
				return -1;

			while (!feof(p) && fscanf(p, "%31s", token)) {
				if (!strcmp(token, "dev")) {
					fscanf(p, "%31s", token);
					break;
				}
			}

			return (WEXITSTATUS(fclose(p))) ? -1 : if_nametoindex(token);
		}

		case AF_PACKET: {
			struct sockaddr_ll *sll = (struct sockaddr_ll *) sa;
			return sll->sll_ifindex;
		}
		
		default:
			return -1;
	}
}

int if_getirqs(struct interface *i)
{
	char dirname[NAME_MAX];
	int irq, n = 0;

	snprintf(dirname, sizeof(dirname), "/sys/class/net/%s/device/msi_irqs/", i->name);
	DIR *dir = opendir(dirname);
	if (dir) {
		memset(&i->irqs, 0, sizeof(char) * IF_IRQ_MAX);

		struct dirent *entry;
		while ((entry = readdir(dir)) && n < IF_IRQ_MAX) {
			if ((irq = atoi(entry->d_name)))
				i->irqs[n++] = irq;
		}

		closedir(dir);
	}

	return 0;
}

int if_setaffinity(struct interface *i, int affinity)
{
	char filename[NAME_MAX];
	FILE *file;

	for (int n = 0; n < IF_IRQ_MAX && i->irqs[n]; n++) {
		snprintf(filename, sizeof(filename), "/proc/irq/%u/smp_affinity", i->irqs[n]);

		file = fopen(filename, "w");
		if (file) {
			if (fprintf(file, "%8x", affinity) < 0)
				error("Failed to set affinity for IRQ %u", i->irqs[n]);

			fclose(file);
			debug(5, "Set affinity of IRQ %u for interface '%s' to %#x", i->irqs[n], i->name, affinity);
		}
		else
			error("Failed to set affinity for interface '%s'", i->name);
	}

	return 0;
}

struct interface * if_lookup_index(int index)
{
	FOREACH(&interfaces, it) {
		if (it->interface->index == index)
			return it->interface;
	}

	return NULL;
}

