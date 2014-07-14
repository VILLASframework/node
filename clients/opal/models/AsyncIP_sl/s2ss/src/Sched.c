/** Configure Scheduler
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Mathieu Dubé-Dallaire
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @copyright 2003, OPAL-RT Technologies inc
 * @file
 */

#include "Sched.h"

#if defined(__QNXNTO__)
#       include <process.h>
#       include <sys/sched.h>
#       include <devctl.h>
#       include <sys/dcmd_chr.h>
#elif defined(__linux__)
#       define _GNU_SOURCE   1
#       include <sched.h>
#	if defined(__redhawk__)
#		include <cpuset.h>
#		include <mpadvise.h>
#	endif
#endif

int AssignProcToCpu0(void)
{
#if defined(__linux__)
    #if defined(__redhawk__)
	int			rc;
	pid_t			pid = getpid();
	cpuset_t		*pCpuset;

	pCpuset = cpuset_alloc();
	if (NULL == pCpuset) {
		fprintf(stderr, "Error allocating a cpuset\n");
		return ENOMEM;
	}

	cpuset_init(pCpuset);
	cpuset_set_cpu(pCpuset, 0, 1);

	rc = mpadvise(MPA_PRC_SETBIAS, MPA_TID, pid, pCpuset);
	if (MPA_FAILURE == rc) {
		rc = errno;
		fprintf(stderr, "Error from mpadvise, %d %s, for pid %d\n", errno, strerror(errno), pid);
		cpuset_free(pCpuset);
		return rc;
	}

	cpuset_free(pCpuset);
	return EOK;
    #else
	cpu_set_t bindSet;
	CPU_ZERO(&bindSet);
	CPU_SET(0, &bindSet);

	/* changing process cpu affinity */
	if (sched_setaffinity(0, sizeof(cpu_set_t), &bindSet) != 0) {
		fprintf(stderr, "Unable to bind the process to CPU 0. (sched_setaffinity errno %d)\n", errno );
		return EINVAL;
	}
	return EOK;
    #endif
#endif	/* __linux__ */
}
