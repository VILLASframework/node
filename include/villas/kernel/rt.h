/** Linux specific real-time optimizations
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup fpga Kernel @{ */

#pragma once

int rt_init(int priority, int affinity);

int rt_set_affinity(int affinity);

int rt_set_priority(int priority);

/** Checks for realtime (PREEMPT_RT) patched kernel.
 *
 * See https://rt.wiki.kernel.org
 *
 * @retval 0 Kernel is patched.
 * @reval <>0 Kernel is not patched.
 */
int rt_is_preemptible();

/** @} */