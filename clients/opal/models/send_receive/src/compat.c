/* Compatibility code for GCC
 *
 * OPAL-RT's libSystem.a links against some Intel
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

size_t __intel_sse2_strlen(const char *s)
{
	return strlen(s);
}

void * _intel_fast_memset(void *b, int c, size_t len)
{
	return memset(b, c, len);
}

void * _intel_fast_memcpy(void *restrict dst, const void *restrict src, size_t n)
{
	return memcpy(dst, src, n);
}

int _intel_fast_memcmp(const void *s1, const void *s2, size_t n)
{
	return memcmp(s1, s2, n);
}

void * _intel_fast_memmove(void *dest, const void *src, size_t num)
{
	return memmove(dest, src, num);
}
