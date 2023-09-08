/* GPU Kernels.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <cuda_runtime.h>

namespace villas {
namespace gpu {

__global__ void
kernel_mailbox(volatile uint32_t *mailbox, volatile uint32_t* counter);

__global__ void
kernel_memcpy(volatile uint8_t* dst, volatile uint8_t* src, size_t length);

} // namespace villas
} // namespace gpu
