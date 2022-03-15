/** Linux specific real-time optimizations
 *
 * @see: https://wiki.linuxfoundation.org/realtime/documentation/howto/applications/application_base
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

} /* namespace villas */
} /* namespace kernel */
} /* namespace rt */
