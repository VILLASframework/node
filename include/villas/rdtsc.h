/** Measure time and sleep with IA-32 time-stamp counter.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <stdint.h>
#include <cpuid.h>

#include <stdio.h>

#ifdef __APPLE__
  #include <sys/types.h>
  #include <sys/sysctl.h>
#endif

#define bit_TSC_INVARIANT	(1 << 8)
#define bit_RDTSCP		(1 << 27)

#if !(__x86_64__ || __i386__)
  #error this header is for x86 only
#endif

/** Get CPU timestep counter */
__attribute__((always_inline))
static inline uint64_t rdtscp()
{
	uint64_t tsc;

	__asm__ __volatile__("rdtscp;"
		 "shl $32, %%rdx;"
		 "or %%rdx,%%rax"
		: "=a" (tsc)
		:
		: "%rcx", "%rdx", "memory");

	return tsc;
}

int rdtsc_init(uint64_t *freq)
{
	uint32_t eax, ebx, ecx, edx;

	/** Check if TSC is supported */
	__get_cpuid(0x1, &eax, &ebx, &ecx, &edx);
	if (!(edx & bit_TSC))
		return -1;

	/** Check if RDTSCP instruction is supported */
	__get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
	if (!(edx & bit_RDTSCP))
		return -1;

	/** Check if TSC is invariant */
	__get_cpuid(0x80000007, &eax, &ebx, &ecx, &edx);
	if (!(edx & bit_TSC_INVARIANT))
		return -1;

	/** Intel SDM Vol 3, Section 18.7.3:
	 * Nominal TSC frequency = CPUID.15H.ECX[31:0] * CPUID.15H.EBX[31:0] ) ÷ CPUID.15H.EAX[31:0]
	 */
	__get_cpuid(0x15, &eax, &ebx, &ecx, &edx);

	if (ecx != 0)
		*freq = ecx * ebx / eax;
	else {
		int ret;
#ifdef __linux__
		FILE *f = fopen(SYSFS_PATH "/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
		if (!f)
			return -1;

		ret = fscanf(f, "%d", freq);

		fclose(f);

		if (ret != 1)
			return -1;
#elif defined(__APPLE__)
		int64_t tscfreq;
		size_t lenp = sizeof(tscfreq);

		ret = sysctlbyname("machdep.tsc.frequency", &tscfreq, &lenp, NULL, 0);
		if (ret)
			return ret;

		*freq = tscfreq;
#endif
	}

	return 0;
}
