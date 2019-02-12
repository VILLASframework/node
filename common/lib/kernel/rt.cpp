/** Linux specific real-time optimizations
 *
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

#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>

#include <villas/log.hpp>
#include <villas/cpuset.hpp>
#include <villas/config.h>
#include <villas/utils.h>
#include <villas/exceptions.hpp>

#include <villas/kernel/kernel.h>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/rt.hpp>

#ifdef __linux__
  using villas::utils::CpuSet;
#endif /* __linux__ */

namespace villas {
namespace kernel {
namespace rt {

int init(int priority, int affinity)
{
	Logger logger = logging.get("kernel:rt");

	logger->info("Initialize sub-system");

#ifdef __linux__
	int is_rt;

	/* Use FIFO scheduler with real time priority */
	is_rt = isPreemptible();
	if (is_rt)
		logger->warn("We recommend to use an PREEMPT_RT patched kernel!");

	if (priority)
		setPriority(priority);
	else
		logger->warn("You might want to use the 'priority' setting to increase " PROJECT_NAME "'s process priority");

	if (affinity)
		setAffinity(affinity);
	else
		logger->warn("You might want to use the 'affinity' setting to pin " PROJECT_NAME " to dedicate CPU cores");

	lockMemory();
#else
	logger->warn("This platform is not optimized for real-time execution");
	(void) affinity;
	(void) priority;
#endif

	return 0;
}

#ifdef __linux__

int setAffinity(int affinity)
{
	char isolcpus[255];
	int is_isol, ret;

	Logger logger = logging.get("kernel:rt");

	/* Pin threads to CPUs by setting the affinity */
	CpuSet cset_pin, cset_isol, cset_non_isol;

	cset_pin = CpuSet(affinity);

	is_isol = kernel_get_cmdline_param("isolcpus", isolcpus, sizeof(isolcpus));
	if (is_isol) {
		logger->warn("You should reserve some cores for " PROJECT_NAME " (see 'isolcpus')");

		cset_isol.zero();
	}
	else {
		cset_isol = CpuSet(isolcpus);
		cset_non_isol = cset_isol ^ cset_pin;

		if (cset_non_isol.count() > 0)
			logger->warn("Affinity setting includes cores which are not isolated: affinity={} isolcpus={}", (std::string) cset_pin, (std::string) cset_isol);
	}

	ret = sched_setaffinity(0, cset_pin.size(), cset_pin);
	if (ret)
		throw SystemError("Failed to set CPU affinity to {}", (std::string) cset_pin);

	logger->debug("Set affinity to {}", (std::string) cset_pin);

	return 0;
}

int setPriority(int priority)
{
	int ret;
	struct sched_param param = {
		.sched_priority = priority
	};

	Logger logger = logging.get("kernel:rt");

	ret = sched_setscheduler(0, SCHED_FIFO, &param);
	if (ret)
		throw SystemError("Failed to set real time priority");

	logger->debug("Task priority set to {}", priority);

	return 0;
}

int isPreemptible()
{
	return access(SYSFS_PATH "/kernel/realtime", R_OK);
}

#endif /* __linux__ */

} // namespace villas
} // namespace kernel
} // namespace rt
