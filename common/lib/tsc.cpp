/** Measure time and sleep with IA-32 time-stamp counter.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <villas/tsc.hpp>

using namespace villas;

int tsc_init(struct Tsc *t)
{
	uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

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
		ret = kernel::get_cpu_frequency(&t->frequency);
		if (ret)
			return ret;
#endif
	}

	return 0;
}

uint64_t tsc_rate_to_cycles(struct Tsc *t, double rate)
{
	return t->frequency / rate;
}
