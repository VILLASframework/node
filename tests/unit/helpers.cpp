/* Unit test helpers.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <criterion/criterion.h>

#include "helpers.hpp"

char * cr_strdup(const char *str)
{
	char *ptr = (char *) cr_malloc(strlen(str) + 1);
	if (ptr)
		strcpy(ptr, str);
	return ptr;
}
