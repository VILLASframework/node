/** Configure scheduler.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Mathieu Dubé-Dallaire
 * @copyright 2003, OPAL-RT Technologies inc
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <errno.h>
#include <sched.h>

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "OpalPrint.h"

#include "config.h"
#include "utils.h"

int AssignProcToCpu0(void)
{
	int ret;
	cpu_set_t bindSet;
	
	CPU_ZERO(&bindSet);
	CPU_SET(0, &bindSet);

	/* Changing process cpu affinity */
	ret = sched_setaffinity(0, sizeof(cpu_set_t), &bindSet);
	if (ret) {
		OpalPrint("Unable to bind the process to CPU 0: %d\n", errno);
		return EINVAL;
	}

	return 0;
}
