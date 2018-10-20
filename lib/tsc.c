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

#include <villas/tsc.h>

int tsc_init(struct tsc *t)
{
	uint32_t eax, ebx, ecx, edx;

	/** Check if TSC is supported */
	__get_cpuid(0x1, &eax, &ebx, &ecx, &edx);
	if (!(edx & bit_TSC))
		return -2;

	/** Check if RDTSCP instruction is supported */
	__get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
	t->rdtscp_supported = edx & bit_RDTSCP;

	/** Check if TSC is invariant */
	__get_cpuid(0x80000007, &eax, &ebx, &ecx, &edx);
	t->is_invariant = edx & bit_TSC_INVARIANT;

	/** Intel SDM Vol 3, Section 18.7.3:
	 * Nominal TSC frequency = CPUID.15H.ECX[31:0] * CPUID.15H.EBX[31:0] ) ÷ CPUID.15H.EAX[31:0]
	 */
	__get_cpuid(0x15, &eax, &ebx, &ecx, &edx);

	if (ecx != 0)
		t->frequency = ecx * ebx / eax;
	else {
		int ret;
#ifdef __linux__
		ret = kernel_get_cpu_frequency(&t->frequency);
		if (ret)
			return ret;
#elif defined(__APPLE__)
		int64_t frequency;
		size_t lenp = sizeof(frequency);

		/** @todo: machdep.tsc.frequency seems to be a measured frequency (based on local APIC?
		 *         We should figure out which frequency is more accurate */
//		ret = sysctlbyname("hw.cpufrequency", &frequency, &lenp, NULL, 0);
		ret = sysctlbyname("machdep.tsc.frequency", &frequency, &lenp, NULL, 0);
		if (ret)
			return ret;

		t->frequency = frequency;
#endif
	}

	return 0;
}

uint64_t tsc_rate_to_cycles(struct tsc *t, double rate)
{
	return t->frequency / rate;
}
