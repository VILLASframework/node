/** Interface related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <linux/if_packet.h>

#include <netlink/route/link.h>

#include <villas/config.h>
#include <villas/utils.h>

#include <villas/kernel/if.h>
#include <villas/kernel/tc.h>
#include <villas/kernel/tc_netem.h>
#include <villas/kernel/nl.h>
#include <villas/kernel/kernel.h>

#include <villas/nodes/socket.h>

int if_init(struct interface *i, struct rtnl_link *link)
{
	i->nl_link = link;

	debug(LOG_IF | 3, "Created interface '%s'", rtnl_link_get_name(i->nl_link));

	int  n = if_get_irqs(i);
	if (n > 0)
		debug(6, "Found %u IRQs for interface '%s'", n, rtnl_link_get_name(i->nl_link));
	else
		warn("Did not found any interrupts for interface '%s'", rtnl_link_get_name(i->nl_link));

	list_init(&i->sockets);

	return 0;
}

int if_destroy(struct interface *i)
{
	/* List members are freed by the nodes they belong to. */
	list_destroy(&i->sockets, NULL, false);

	rtnl_qdisc_put(i->tc_qdisc);

	free(i);

	return 0;
}

int if_start(struct interface *i)
{
	info("Starting interface '%s' which is used by %zu sockets", rtnl_link_get_name(i->nl_link), list_length(&i->sockets));

	{ INDENT
		/* Set affinity for network interfaces (skip _loopback_ dev) */
		//if_set_affinity(i, i->affinity);

		/* Assign fwmark's to socket nodes which have netem options */
		int ret, mark = 0;
		for (size_t j = 0; j < list_length(&i->sockets); j++) {
			struct socket *s = (struct socket *) list_at(&i->sockets, j);

			if (s->tc_qdisc)
				s->mark = 1 + mark++;
		}

		/* Abort if no node is using netem */
		if (mark == 0)
			return 0;

		if (getuid() != 0)
			error("Network emulation requires super-user privileges!");

		/* Replace root qdisc */
		ret = tc_prio(i, &i->tc_qdisc, TC_HANDLE(1, 0), TC_H_ROOT, mark);
		if (ret)
			error("Failed to setup priority queuing discipline: %s", nl_geterror(ret));

		/* Create netem qdisks and appropriate filter per netem node */
		for (size_t j = 0; j < list_length(&i->sockets); j++) {
			struct socket *s = (struct socket *) list_at(&i->sockets, j);

			if (s->tc_qdisc) {
				ret = tc_mark(i,  &s->tc_classifier, TC_HANDLE(1, s->mark), s->mark);
				if (ret)
					error("Failed to setup FW mark classifier: %s", nl_geterror(ret));

				char *buf = tc_netem_print(s->tc_qdisc);
				debug(LOG_IF | 5, "Starting network emulation on interface '%s' for FW mark %u: %s",
					rtnl_link_get_name(i->nl_link), s->mark, buf);
				free(buf);

				ret = tc_netem(i, &s->tc_qdisc, TC_HANDLE(0x1000+s->mark, 0), TC_HANDLE(1, s->mark));
				if (ret)
					error("Failed to setup netem qdisc: %s", nl_geterror(ret));
			}
		}
	}

	return 0;
}

int if_stop(struct interface *i)
{
	info("Stopping interface '%s'", rtnl_link_get_name(i->nl_link));

	{ INDENT
		if_set_affinity(i, -1L);

		if (i->tc_qdisc)
			tc_reset(i);
	}

	return 0;
}

int if_get_egress(struct sockaddr *sa, struct rtnl_link **link)
{
	int ifindex = -1;

	switch (sa->sa_family) {
		case AF_INET:
		case AF_INET6: {
			struct sockaddr_in *sin = (struct sockaddr_in *) sa;
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;

			struct nl_addr *addr = (sa->sa_family == AF_INET)
				? nl_addr_build(sin->sin_family, &sin->sin_addr.s_addr, sizeof(sin->sin_addr.s_addr))
				: nl_addr_build(sin6->sin6_family, sin6->sin6_addr.s6_addr, sizeof(sin6->sin6_addr));

			ifindex = nl_get_egress(addr); nl_addr_put(addr);
			if (ifindex < 0)
				error("Netlink error: %s", nl_geterror(ifindex));
			break;
		}

		case AF_PACKET: {
			struct sockaddr_ll *sll = (struct sockaddr_ll *) sa;

			ifindex = sll->sll_ifindex;
			break;
		}
	}

	struct nl_cache *cache = nl_cache_mngt_require("route/link");
	*link = rtnl_link_get(cache, ifindex);
	if (!*link)
		return -1;

	return 0;
}

int if_get_irqs(struct interface *i)
{
	char dirname[NAME_MAX];
	int irq, n = 0;

	snprintf(dirname, sizeof(dirname), "/sys/class/net/%s/device/msi_irqs/", rtnl_link_get_name(i->nl_link));
	DIR *dir = opendir(dirname);
	if (dir) {
		memset(&i->irqs, 0, sizeof(char) * IF_IRQ_MAX);

		struct dirent *entry;
		while ((entry = readdir(dir)) && n < IF_IRQ_MAX) {
			irq = atoi(entry->d_name);
			if (irq)
				i->irqs[n++] = irq;
		}

		closedir(dir);
	}

	return 0;
}

int if_set_affinity(struct interface *i, int affinity)
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
			debug(LOG_IF | 5, "Set affinity of IRQ %u for interface '%s' to %#x", i->irqs[n], rtnl_link_get_name(i->nl_link), affinity);
		}
		else
			error("Failed to set affinity for interface '%s'", rtnl_link_get_name(i->nl_link));
	}

	return 0;
}
