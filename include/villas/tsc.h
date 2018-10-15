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

#include <stdbool.h>
#include <cpuid.h>
#include <inttypes.h>

#include <villas/kernel/kernel.h>

#ifdef __APPLE__
  #include <sys/types.h>
  #include <sys/sysctl.h>
#endif

#ifndef bit_TSC
  #define bit_TSC (1 << 4)
#endif

#define bit_TSC_INVARIANT	(1 << 8)
#define bit_RDTSCP		(1 << 27)

#if !(__x86_64__ || __i386__)
  #error this header is for x86 only
#endif

struct tsc {
	uint64_t frequency;

	bool rdtscp_supported;
	bool is_invariant;
};

/** Get CPU timestep counter */
__attribute__((unused,always_inline))
static inline uint64_t rdtscp()
{
	uint64_t tsc;

	__asm__ __volatile__(
		 "rdtscp;"
		 "shl $32, %%rdx;"
		 "or %%rdx,%%rax"
		: "=a" (tsc)
		:
		: "%rcx", "%rdx", "memory"
	);

	return tsc;
}

__attribute__((unused,always_inline))
static inline uint64_t rdtsc()
{
	uint64_t tsc;

	__asm__ __volatile__(
		 "lfence;"
		 "rdtsc;"
		 "shl $32, %%rdx;"
		 "or %%rdx,%%rax"
		: "=a" (tsc)
		:
		: "%rcx", "%rdx", "memory"
	);

	return tsc;
}

int tsc_init(struct tsc *t);

uint64_t tsc_rate_to_cycles(struct tsc *t, double rate);

uint64_t tsc_now(struct tsc *t);
