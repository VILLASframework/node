/** Linux kernel related functions.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <cstddef>

#include <villas/version.hpp>

namespace villas {
namespace kernel {

/** Get the version of the kernel. */
utils::Version getVersion();

/** Get number of reserved hugepages. */
int getNrHugepages();

/** Set number of reserved hugepages. */
int setNrHugepages(int nr);

/** Get kernel cmdline parameter
 *
 * See https://www.kernel.org/doc/Documentation/kernel-parameters.txt
 *
 * @param param The cmdline parameter to look for.
 * @param buf The string buffer to which the parameter value will be copied to.
 * @param len The length of the buffer \p value
 * @retval 0 Parameter \p key was found and value was copied to \p value
 * @reval <>0 Kernel was not booted with parameter \p key
 */
int getCmdlineParam(const char *param, char *buf, size_t len);

/** Checks if a kernel module is loaded
 *
 * @param module the name of the module
 * @retval 0 Module is loaded.
 * @reval <>0 Module is not loaded.
 */
int isModuleLoaded(const char *module);

/** Load kernel module via modprobe */
int loadModule(const char *module);

/** Set parameter of loaded kernel module */
int setModuleParam(const char *module, const char *param, const char *value);

/** Get cacheline size in bytes */
int getCachelineSize();

/** Get the size of a standard page in bytes. */
int getPageSize();

/** Get the size of a huge page in bytes. */
int getHugePageSize();

/** Get CPU base frequency */
int get_cpu_frequency(uint64_t *freq);

/** Set SMP affinity of IRQ */
int setIRQAffinity(unsigned irq, uintmax_t aff , uintmax_t *old);

} // namespace villas
} // namespace kernel
