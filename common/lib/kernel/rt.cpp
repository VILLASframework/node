/* Linux specific real-time optimizations.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sched.h>
#include <unistd.h>

#include <villas/config.hpp>
#include <villas/cpuset.hpp>
#include <villas/exceptions.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

#ifdef __linux__
using villas::utils::CpuSet;
#endif // __linux__

namespace villas {
namespace kernel {
namespace rt {

void init(int priority, int affinity) {
  Logger logger = Log::get("kernel:rt");

  logger->info("Initialize sub-system");

#ifdef __linux__
  int is_rt, is_isol;
  char isolcpus[255];

  // Use FIFO scheduler with real time priority
  is_rt = isPreemptible();
  if (!is_rt)
    logger->warn("We recommend to use an PREEMPT_RT patched kernel!");

  if (priority)
    setPriority(priority);
  else

    logger->warn("You might want to use the 'priority' setting to "
                 "increase " PROJECT_NAME // cppcheck-suppress unknownMacro
                 "'s process priority");

  if (affinity) {
    is_isol = getCmdlineParam("isolcpus", isolcpus, sizeof(isolcpus));
    if (is_isol)
      // cppcheck-suppress unknownMacro
      logger->warn("You should reserve some cores for " PROJECT_NAME
                   " (see 'isolcpus')");
    else {
      CpuSet cset_pin(affinity);
      CpuSet cset_isol(isolcpus);
      CpuSet cset_non_isol = ~cset_isol & cset_pin;

      if (cset_non_isol.count() > 0)
        logger->warn("Affinity setting includes cores which are not isolated: "
                     "affinity={}, isolcpus={}, non_isolated={}",
                     (std::string)cset_pin, (std::string)cset_isol,
                     (std::string)cset_non_isol);
    }

    setProcessAffinity(affinity);
  } else
    // cppcheck-suppress unknownMacro
    logger->warn(
        "You might want to use the 'affinity' setting to pin " PROJECT_NAME
        " to dedicate CPU cores");
#else
  logger->warn("This platform is not optimized for real-time execution");

  (void)affinity;
  (void)priority;
#endif // __linux__
}

#ifdef __linux__
void setProcessAffinity(int affinity) {
  int ret;

  assert(affinity != 0);

  Logger logger = Log::get("kernel:rt");

  // Pin threads to CPUs by setting the affinity
  CpuSet cset_pin(affinity);

  ret = sched_setaffinity(0, cset_pin.size(), cset_pin);
  if (ret)
    throw SystemError("Failed to set CPU affinity of process");

  logger->debug("Set affinity to {} {}",
                cset_pin.count() == 1 ? "core" : "cores",
                (std::string)cset_pin);
}

void setThreadAffinity(pthread_t thread, int affinity) {
  int ret;

  assert(affinity != 0);

  Logger logger = Log::get("kernel:rt");

  CpuSet cset_pin(affinity);

  ret = pthread_setaffinity_np(thread, cset_pin.size(), cset_pin);
  if (ret)
    throw SystemError("Failed to set CPU affinity of thread");

  logger->debug("Set affinity of thread {} to {} {}", (long unsigned)thread,
                cset_pin.count() == 1 ? "core" : "cores",
                (std::string)cset_pin);
}

void setPriority(int priority) {
  int ret;
  struct sched_param param;
  param.sched_priority = priority;

  Logger logger = Log::get("kernel:rt");

  ret = sched_setscheduler(0, SCHED_FIFO, &param);
  if (ret)
    throw SystemError("Failed to set real time priority");

  logger->debug("Task priority set to {}", priority);
}

bool isPreemptible() {
  return access(SYSFS_PATH "/kernel/realtime", R_OK) == 0;
}

#endif // __linux__

} // namespace rt
} // namespace kernel
} // namespace villas
