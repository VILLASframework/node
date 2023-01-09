/** Memory managment.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <sys/mman.h>
#include <unistd.h>

#include <villas/memory.hpp>

using namespace villas;

bool
HostRam::free(void* addr, size_t length)
{
	return munmap(addr, length) == 0;
}

void*
HostRam::allocate(size_t length, int flags)
{
	const int mmap_flags = flags | MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT;
	const int mmap_protection = PROT_READ | PROT_WRITE;

	return mmap(nullptr, length, mmap_protection, mmap_flags, 0, 0);
}
