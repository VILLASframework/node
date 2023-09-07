/* Time related functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <cstdio>

#include <ctime>

// Compare two timestamps. Return zero if they are equal.
ssize_t time_cmp(const struct timespec *a, const struct timespec *b);

// Get delta between two timespec structs.
struct timespec time_diff(const struct timespec *start,
                          const struct timespec *end);

// Get sum of two timespec structs.
struct timespec time_add(const struct timespec *start,
                         const struct timespec *end);

// Return current time as a struct timespec.
struct timespec time_now();

// Return the diffrence off two timestamps as double value in seconds.
double time_delta(const struct timespec *start, const struct timespec *end);

// Convert timespec to double value representing seconds.
double time_to_double(const struct timespec *ts);

// Convert double containing seconds after 1970 to timespec.
struct timespec time_from_double(double secs);
