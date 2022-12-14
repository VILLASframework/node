/** Linux specific real-time optimizations
 *
 * @see: https://wiki.linuxfoundation.org/realtime/documentation/howto/applications/application_base
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <pthread.h>

namespace villas {
namespace kernel {
namespace rt {

void init(int priority, int affinity);

void setProcessAffinity(int affinity);
void setThreadAffinity(pthread_t thread, int affinity);

void setPriority(int priority);

/** Checks for realtime (PREEMPT_RT) patched kernel.
 *
 * See https://rt.wiki.kernel.org
 *
 * @retval true Kernel is patched.
 * @retval false Kernel is not patched.
 */
bool isPreemptible();

} // namespace villas
} // namespace kernel
} // namespace rt
