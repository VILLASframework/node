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
