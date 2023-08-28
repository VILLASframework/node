/* Measure time and sleep with IA-32 time-stamp counter.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 */

#pragma once

#if !(__x86_64__ || __i386__)
  #error this header is for x86 only
#endif

#include <cpuid.h>
#include <cinttypes>
#include <x86intrin.h>

#include <villas/kernel/kernel.hpp>

#ifndef bit_TSC
  #define bit_TSC (1 << 4)
#endif

#define bit_TSC_INVARIANT	(1 << 8)
#define bit_RDTSCP		(1 << 27)

struct Tsc {
	uint64_t frequency;

	bool rdtscp_supported;
	bool is_invariant;
};

__attribute__((unused)) static
uint64_t tsc_now(struct Tsc *t)
{
	uint32_t tsc_aux;
	return t->rdtscp_supported
		? __rdtscp(&tsc_aux)
		: __rdtsc();
}

int tsc_init(struct Tsc *t) __attribute__ ((warn_unused_result));

uint64_t tsc_rate_to_cycles(struct Tsc *t, double rate);
