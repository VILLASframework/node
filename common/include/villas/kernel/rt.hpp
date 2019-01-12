/** Linux specific real-time optimizations
 *
 * @see: https://wiki.linuxfoundation.org/realtime/documentation/howto/applications/application_base
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup kernel Kernel
 * @{
 */

#pragma once

namespace villas {
namespace kernel {
namespace rt {

int init(int priority, int affinity);

int setAffinity(int affinity);

int setPriority(int priority);

int lockMemory();

/** Checks for realtime (PREEMPT_RT) patched kernel.
 *
 * See https://rt.wiki.kernel.org
 *
 * @retval 0 Kernel is patched.
 * @reval <>0 Kernel is not patched.
 */
int isPreemptible();

} // namespace villas
} // namespace kernel
} // namespace rt

/** @} */
