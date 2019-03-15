/** Interface related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node/config.h>
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

	debug(LOG_IF | 3, "Created interface '%s'", if_name(i));

	int  n = if_get_irqs(i);
	if (n > 0)
		debug(6, "Found %u IRQs for interface '%s'", n, if_name(i));
	else
		warning("Did not found any interrupts for interface '%s'", if_name(i));

	vlist_init(&i->nodes);

	return 0;
}

int if_destroy(struct interface *i)
{
	/* List members are freed by the nodes they belong to. */
	vlist_destroy(&i->nodes, NULL, false);

	rtnl_qdisc_put(i->tc_qdisc);

	return 0;
}

int if_start(struct interface *i)
{
	info("Starting interface '%s' which is used by %zu nodes", if_name(i), vlist_length(&i->nodes));

	/* Set affinity for network interfaces (skip _loopback_ dev) */
	//if_set_affinity(i, i->affinity);

	/* Assign fwmark's to nodes which have netem options */
	int ret, fwmark = 0;
	for (size_t j = 0; j < vlist_length(&i->nodes); j++) {
		struct node *n = (struct node *) vlist_at(&i->nodes, j);

		if (n->tc_qdisc && n->fwmark < 0)
			n->fwmark = 1 + fwmark++;
	}

	/* Abort if no node is using netem */
	if (fwmark == 0)
		return 0;

	if (getuid() != 0)
		error("Network emulation requires super-user privileges!");

	/* Replace root qdisc */
	ret = tc_prio(i, &i->tc_qdisc, TC_HANDLE(1, 0), TC_H_ROOT, fwmark);
	if (ret)
		error("Failed to setup priority queuing discipline: %s", nl_geterror(ret));

	/* Create netem qdisks and appropriate filter per netem node */
	for (size_t j = 0; j < vlist_length(&i->nodes); j++) {
		struct node *n = (struct node *) vlist_at(&i->nodes, j);

		if (n->tc_qdisc) {
			ret = tc_mark(i,  &n->tc_classifier, TC_HANDLE(1, n->fwmark), n->fwmark);
			if (ret)
				error("Failed to setup FW mark classifier: %s", nl_geterror(ret));

			char *buf = tc_netem_print(n->tc_qdisc);
			debug(LOG_IF | 5, "Starting network emulation on interface '%s' for FW mark %u: %s",
					if_name(i), n->fwmark, buf);
			free(buf);

			ret = tc_netem(i, &n->tc_qdisc, TC_HANDLE(0x1000+n->fwmark, 0), TC_HANDLE(1, n->fwmark));
			if (ret)
				error("Failed to setup netem qdisc: %s", nl_geterror(ret));
		}
	}

	return 0;
}

int if_stop(struct interface *i)
{
	info("Stopping interface '%s'", if_name(i));

	//if_set_affinity(i, -1L);

	if (i->tc_qdisc)
		tc_reset(i);

	return 0;
}

const char * if_name(struct interface *i)
{
	return rtnl_link_get_name(i->nl_link);
}

struct interface * if_get_egress(struct sockaddr *sa, struct vlist *interfaces)
{
	int ret;
	struct rtnl_link *link;

	/* Determine outgoing interface */
	link = if_get_egress_link(sa);
	if (!link) {
		char *buf = socket_print_addr(sa);
		error("Failed to get interface for socket address '%s'", buf);
		free(buf);

		return NULL;
	}

	/* Search of existing interface with correct ifindex */
	struct interface *i;
	for (size_t k = 0; k < vlist_length(interfaces); k++) {
		i = (struct interface *) vlist_at(interfaces, k);

		if (rtnl_link_get_ifindex(i->nl_link) == rtnl_link_get_ifindex(link))
			return i;
	}

	/* If not found, create a new interface */
	i = alloc(sizeof(struct interface));
	if (!i)
		return NULL;

	ret = if_init(i, link);
	if (ret)
		NULL;

	vlist_push(interfaces, i);

	return i;
}

struct rtnl_link * if_get_egress_link(struct sockaddr *sa)
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

	return rtnl_link_get(cache, ifindex);
}

int if_get_irqs(struct interface *i)
{
	char dirname[NAME_MAX];
	int irq, n = 0;

	snprintf(dirname, sizeof(dirname), "/sys/class/net/%s/device/msi_irqs/", if_name(i));
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
			debug(LOG_IF | 5, "Set affinity of IRQ %u for interface '%s' to %#x", i->irqs[n], if_name(i), affinity);
		}
		else
			error("Failed to set affinity for interface '%s'", if_name(i));
	}

	return 0;
}
