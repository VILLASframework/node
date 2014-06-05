/**
 * Realtime related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>
#include <sched.h>

#include "config.h"
#include "utils.h"

void rt_stack_prefault()
{
	unsigned char dummy[MAX_SAFE_STACK];

	memset(dummy, 0, MAX_SAFE_STACK);
	
	
}

void rt_set_priority()
{
	struct sched_param param;

	param.sched_priority = PRIORITY;
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
                perror("sched_setscheduler failed");
                exit(-1);
        }
}

void rt_lock_memory()
{
	// TODO
}

void rt_set_affinity()
{
	// TODO
}

void rt_init()
{
	rt_set_priority();
	rt_set_affinity();
	rt_lock_memory();
	rt_stack_prefault();
}
