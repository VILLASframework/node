/* GPU managment.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sstream>

#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/memory_manager.hpp>
#include <villas/plugin.hpp>

namespace villas {
namespace gpu {

class GpuAllocator;

class Gpu {
  friend GpuAllocator;

public:
  Gpu(int gpuId);
  ~Gpu();

  bool init();

  std::string getName() const;

  GpuAllocator &getAllocator() const { return *allocator; }

  bool makeAccessibleToPCIeAndVA(const MemoryBlock &mem);

  // Make some memory block accssible for this GPU
  bool makeAccessibleFromPCIeOrHostRam(const MemoryBlock &mem);

  void memcpySync(const MemoryBlock &src, const MemoryBlock &dst, size_t size);

  void memcpyKernel(const MemoryBlock &src, const MemoryBlock &dst,
                    size_t size);

  MemoryTranslation translate(const MemoryBlock &dst);

private:
  bool registerIoMemory(const MemoryBlock &mem);
  bool registerHostMemory(const MemoryBlock &mem);

private:
  class impl;
  std::unique_ptr<impl> pImpl;

  // Master, will be used to derived slave addr spaces for allocation
  MemoryManager::AddressSpaceId masterPciEAddrSpaceId;

  MemoryManager::AddressSpaceId slaveMemoryAddrSpaceId;

  Logger logger;

  int gpuId;

  std::unique_ptr<GpuAllocator> allocator;
};

class GpuAllocator : public BaseAllocator<GpuAllocator> {
public:
  static constexpr size_t GpuPageSize = 64UL << 10;

  GpuAllocator(Gpu &gpu);

  std::string getName() const;

  std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
  allocateBlock(size_t size);

private:
  Gpu &gpu;
  // TODO: replace by multimap (key is available memory)
  std::list<std::unique_ptr<LinearAllocator>> chunks;
};

class GpuFactory : public Plugin {
public:
  GpuFactory();

  std::list<std::unique_ptr<Gpu>> make();

  void run(void *);

private:
  Logger logger;
};

} // namespace gpu
} // namespace villas
