/** Configure scheduler.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Mathieu Dubé-Dallaire
 * @copyright 2003, OPAL-RT Technologies inc
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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
