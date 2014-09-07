/** Interface related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "if.h"
#include "utils.h"

int if_getegress(struct sockaddr_in *sa)
{
	char cmd[128];
	char token[32];

	snprintf(cmd, sizeof(cmd), "ip route get %s", inet_ntoa(sa->sin_addr));

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

int if_getirqs(struct interface *i)
{
	char dirname[NAME_MAX];

	snprintf(dirname, sizeof(dirname), "/sys/class/net/%s/device/msi_irqs/", i->name);
	DIR *dir = opendir(dirname);
	if (!dir) {
		warn("Cannot open IRQs for interface '%s'", i->name);
		return -ENOENT;
	}

	memset(&i->irqs, 0, sizeof(char) * IF_IRQ_MAX);

	int irq, n = 0;
	struct dirent *entry;
	while ((entry = readdir(dir)) && n < IF_IRQ_MAX) {
		if ((irq = atoi(entry->d_name)))
			i->irqs[n++] = irq;
	}

	debug(7, "Found %u interrupts for interface '%s'", n, i->name);

	closedir(dir);
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

struct interface* if_lookup_index(int index, struct interface *interfaces)
{
	for (struct interface *i = interfaces; i; i = i->next) {
		if (i->index == index) {
			return i;
		}
	}

	return NULL;
}
