/**
 * Interface related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <ifaddrs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "utils.h"

char* if_addrtoname(struct sockaddr *addr)
{
	char *ifname = NULL;
	struct ifaddrs *ifas, *ifa;

	if(getifaddrs(&ifas))
		return NULL;

	for(ifa = ifas; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr && !sockaddr_cmp(ifa->ifa_addr, addr)) {
			ifname = malloc(strlen(ifa->ifa_name) + 1);
			if (!ifname)
				goto out;

			strcpy(ifname, ifa->ifa_name);
			goto out;
		}
	}

out:
	freeifaddrs(ifas);

	return ifname;
}

int if_setaffinity(const char *ifname, int affinity)
{
	char *dirname[NAME_MAX];
	char *filename[NAME_MAX];

	DIR *dir;
	FILE *file;

	/* Determine IRQs numbers */
	snprintf(dirname, sizeof(dirname), "/sys/class/net/%s/device/msi_irqs/", ifname);
	dir = opendir(dirname);
	if (!dir)
		return -1;

	struct dirent *entry;
	while (entry = readdir(dir)) {
		/* Set SMP affinity */
		snprintf(filename, sizeof(filename), "/proc/irq/%s/smp_affinity");
		file = fopen(filename, "w");
		if (!file)
			continue;

		debug(3, "Setting SMP affinity of IRQ %s (%s) to %8x\n", entry->d_name, ifname, affinity);

		fprintf("%8x", affinity);
		fclose(file);
	}

	closedir(dir);

	return 0;
}
