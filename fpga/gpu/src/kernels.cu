/** GPU Kernels.
 *
 * @file
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
 
 #include <stdio.h>

#include <villas/gpu.hpp>

#include <cuda_runtime.h>
#include <cuda.h>

#include "kernels.hpp"

namespace villas {
namespace gpu {


__global__ void
kernel_mailbox(volatile uint32_t *mailbox, volatile uint32_t* counter)
{
	printf("[gpu] hello!\n");
	printf("[gpu] mailbox: %p\n", mailbox);

	printf("[kernel] started\n");

	while(1) {
		if (*mailbox == 1) {
			*mailbox = 0;
			printf("[gpu] counter = %d\n", *counter);
			break;
		}
	}

	printf("[gpu] quit\n");
}

__global__ void
kernel_memcpy(volatile uint8_t* dst, volatile uint8_t* src, size_t length)
{
	while(length > 0) {
		*dst++ = *src++;
		length--;
	}
}

} // namespace villas
} // namespace gpu
