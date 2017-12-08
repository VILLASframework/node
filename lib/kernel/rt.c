/** Linux specific real-time optimizations
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <villas/config.h>
#include <villas/utils.h>
#include <villas/super_node.h>

#include <villas/kernel/kernel.h>
#include <villas/kernel/rt.h>

int rt_init(int priority, int affinity)
{
	info("Initialize real-time sub-system");

	{ INDENT
#ifdef __linux__
	int is_rt;

	/* Use FIFO scheduler with real time priority */
	is_rt = rt_is_preemptible();
	if (is_rt)
		warn("We recommend to use an PREEMPT_RT patched kernel!");

	if (priority)
		rt_set_priority(priority);
	else
		warn("You might want to use the 'priority' setting to increase VILLASnode's process priority");

	if (affinity)
		rt_set_affinity(affinity);
	else
		warn("You might want to use the 'affinity' setting to pin VILLASnode to dedicate CPU cores");

	rt_lock_memory();
#else
	warn("This platform is not optimized for real-time execution");
#endif
	}

	return 0;
}

#ifdef __linux__

int rt_lock_memory()
{
	int ret;

#ifdef _POSIX_MEMLOCK
	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret)
		error("Failed to lock memory");
#endif

	return 0;
}

int rt_set_affinity(int affinity)
{
	char isolcpus[255];
	int is_isol, ret;

	/* Pin threads to CPUs by setting the affinity */
	cpu_set_t cset_pin, cset_isol, cset_non_isol;

	cpuset_from_integer(affinity, &cset_pin);

	is_isol = kernel_get_cmdline_param("isolcpus", isolcpus, sizeof(isolcpus));
	if (is_isol) {
		warn("You should reserve some cores for VILLASnode (see 'isolcpus')");

		CPU_ZERO(&cset_isol);
	}
	else {
		ret = cpulist_parse(isolcpus, &cset_isol, 0);
		if (ret)
			error("Invalid isolcpus cmdline parameter: %s", isolcpus);

		CPU_XOR(&cset_non_isol, &cset_isol, &cset_pin);
		if (CPU_COUNT(&cset_non_isol) > 0) {
			char isol[128], pin[128];

			cpulist_create(isol, sizeof(isol), &cset_isol);
			cpulist_create(pin, sizeof(pin), &cset_pin);

			warn("Affinity setting includes cores which are not isolated: affinity=%s isolcpus=%s", pin, isol);
		}
	}

	char list[128];
	cpulist_create(list, sizeof(list), &cset_pin);

	ret = sched_setaffinity(0, sizeof(cpu_set_t), &cset_pin);
	if (ret)
		serror("Failed to set CPU affinity to %s", list);

	debug(LOG_KERNEL | 3, "Set affinity to %s", list);

	return 0;
}

int rt_set_priority(int priority)
{
	int ret;
	struct sched_param param = {
		.sched_priority = priority
	};

	ret = sched_setscheduler(0, SCHED_FIFO, &param);
	if (ret)
		serror("Failed to set real time priority");

	debug(LOG_KERNEL | 3, "Task priority set to %u", priority);

	return 0;
}

int rt_is_preemptible()
{
	return access(SYSFS_PATH "/kernel/realtime", R_OK);
}

#endif /* __linux__ */
