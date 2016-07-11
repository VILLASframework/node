/** Linux specific real-time optimizations
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include <sched.h>

#include "utils.h"
#include "kernel/kernel.h"
#include "kernel/rt.h"

int rt_init(int affinity, int priority)
{ INDENT
	char isolcpus[255];
	int is_isol, is_rt, ret;

	/* Use FIFO scheduler with real time priority */
	is_rt = kernel_is_rt();
	if (is_rt)
		warn("We recommend to use an PREEMPT_RT patched kernel!");

	struct sched_param param = {
		.sched_priority = priority
	};

	if (priority) {
		ret = sched_setscheduler(0, SCHED_FIFO, &param);
		if (ret)
			serror("Failed to set real time priority");

		debug(3, "Task priority set to %u", priority);
	}

	if (affinity) {
		/* Pin threads to CPUs by setting the affinity */
		cpu_set_t cset_pin, cset_isol, cset_non_isol;

		is_isol = kernel_get_cmdline_param("isolcpus", isolcpus, sizeof(isolcpus));
		if (is_isol) {
			warn("You should reserve some cores for the server (see 'isolcpus')");

			CPU_ZERO(&cset_isol);
		}
		else {
			ret = cpulist_parse(isolcpus, &cset_isol, 0);
			if (ret)
				error("Invalid isolcpus cmdline parameter: %s", isolcpus);
		}

		cpuset_from_integer(affinity, &cset_pin);

		CPU_XOR(&cset_non_isol, &cset_isol, &cset_pin);
		if (CPU_COUNT(&cset_non_isol) > 0) {
			char isol[128], pin[128];
		
			cpulist_create(isol, sizeof(isol), &cset_isol);
			cpulist_create(pin, sizeof(pin), &cset_pin);

			warn("Affinity setting includes cores which are not isolated: affinity=%s isolcpus=%s", pin, isol);
		}

		char list[128];
		cpulist_create(list, sizeof(list), &cset_pin);

		ret = sched_setaffinity(0, sizeof(cpu_set_t), &cset_pin);
		if (ret)
			serror("Failed to set CPU affinity to %s", list);

		debug(3, "Set affinity to %s", list);
	}
	else
		warn("You should use the 'affinity' setting to pin VILLASnode to dedicate CPU cores");

	return 0;
}