/** OPAL Interface setup
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#include "Config.h"
#include "Interface.h"

int if_setup(const char *op, const char *iface, const char *addr)
{
	char cmd[256];

	/* Setup remote address */
	snprintf(cmd, 256, "ip addr %s %s/32 dev %s", op, addr, iface);
	if (system(cmd))
		OpalPrint("Failed to add local address to interface");

	/* Setup route for single IP address */
	snprintf(cmd, 256, "ip route %s %s/32 dev %s", op, addr, iface);
	if (system(cmd))
		OpalPrint("Failed to add route for remote address");

	return 0;
}
