/* Measure time and sleep with IA-32 time-stamp counter (x86) or a generic cycle-based timing.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cinttypes>

#if defined(__x86_64__) || defined(__i386__)

#include <cpuid.h>
#include <x86intrin.h>

#include <villas/kernel/kernel.hpp>

#ifndef bit_TSC
#define bit_TSC (1 << 4)
#endif

#define bit_TSC_INVARIANT (1 << 8)
#define bit_RDTSCP (1 << 27)

#else

#include <time.h>

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000L
#endif

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC // Fallback for Darwin/BSD
#endif

#endif

struct Tsc {
  uint64_t frequency;
#if defined(__x86_64__) || defined(__i386__)
  bool rdtscp_supported;
  bool is_invariant;
#endif
};

#if defined(__x86_64__) || defined(__i386__)
__attribute__((unused)) static uint64_t tsc_now(struct Tsc *t) {
  uint32_t tsc_aux;
  return t->rdtscp_supported ? __rdtscp(&tsc_aux) : __rdtsc();
}
#else
// Fallback for CLOCK_MONOTONIC_RAW (Linux/Darwin/BSD specific)
__attribute__((unused)) static uint64_t tsc_now(struct Tsc *t) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return (ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec) * t->frequency / NSEC_PER_SEC;
}
#endif

int tsc_init(struct Tsc *t) __attribute__((warn_unused_result));

uint64_t tsc_rate_to_cycles(struct Tsc *t, double rate);
