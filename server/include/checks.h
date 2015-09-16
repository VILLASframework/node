/** Check system requirements.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#ifndef _CHECKS_H_
#define _CHECKS_H_

/** Checks for realtime (RT_PREEMPT) patched kernel.
 *
 * See https://rt.wiki.kernel.org
 *
 * @retval 0 Kernel is patched.
 * @reval <>0 Kernel is not patched.
 */
int check_kernel_rtpreempt();

/** Check if kernel command line contains "isolcpus=" option.
 *
 * See https://www.kernel.org/doc/Documentation/kernel-parameters.txt
 *
 * @retval 0 Kernel has isolated cores.
 * @reval <>0 Kernel has no isolated cores.
 */
int check_kernel_cmdline();

/** Check if kernel is version is sufficient
 *
 * @retval 0 Kernel version is sufficient.
 * @reval <>0 Kernel version is not sufficient.
 */
int check_kernel_version();

/** Check if program is running with super user (root) privileges.
 *
 *
 * @retval 0 Permissions are sufficient.
 * @reval <>0 Permissions are not sufficient.
 */
int check_root();

/** Simple copy protection.
 *
 * This function checks several machine IDs for predefined values.
 *
 * @retval 0 The machine is allowed to run this program.
 * @reval <>0 The machine is not allowed to run this program.
 */
int check_license_ids();

int check_license_time();

int check_license_trace();

#endif
