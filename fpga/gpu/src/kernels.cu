/** GPU Kernels.
 *
 * @file
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017-2022, Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <stdio.h>

#include <villas/gpu.hpp>

#include <cuda_runtime.h>
#include <cuda.h>

#include "kernels.hpp"

using namespace villas::gpu;


__global__ void
kernel_mailbox(volatile uint32_t *mailbox, volatile uint32_t* counter)
{
	printf("[gpu] hello!\n");
	printf("[gpu] mailbox: %p\n", mailbox);

	printf("[kernel] started\n");

	while (1) {
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
	while (length > 0) {
		*dst++ = *src++;
		length--;
	}
}
