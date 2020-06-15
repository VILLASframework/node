/** Memory managment.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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
