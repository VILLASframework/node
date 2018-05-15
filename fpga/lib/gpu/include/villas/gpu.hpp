#pragma once

#include <sstream>

#include <plugin.hpp>
#include <memory_manager.hpp>
#include <memory.hpp>
#include <villas/log.hpp>


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

	GpuAllocator& getAllocator() const
	{ return *allocator; }


	bool makeAccessibleToPCIeAndVA(const MemoryBlock& mem);

	/// Make some memory block accssible for this GPU
	bool makeAccessibleFromPCIeOrHostRam(const MemoryBlock& mem);

	void memcpySync(const MemoryBlock& src, const MemoryBlock& dst, size_t size);

	void memcpyKernel(const MemoryBlock& src, const MemoryBlock& dst, size_t size);

private:
	bool registerIoMemory(const MemoryBlock& mem);
	bool registerHostMemory(const MemoryBlock& mem);

private:
	class impl;
	std::unique_ptr<impl> pImpl;

	// master, will be used to derived slave addr spaces for allocation
	MemoryManager::AddressSpaceId masterPciEAddrSpaceId;

	MemoryManager::AddressSpaceId slaveMemoryAddrSpaceId;

	SpdLogger logger;

	int gpuId;

	std::unique_ptr<GpuAllocator> allocator;
};


class GpuAllocator : public BaseAllocator<GpuAllocator> {
public:
	GpuAllocator(Gpu& gpu);

	std::string getName() const;

	std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
	allocateBlock(size_t size);

private:
	Gpu& gpu;
};

class GpuFactory : public Plugin {
public:
	GpuFactory();

	std::list<std::unique_ptr<Gpu>>
	make();

	void run(void*);

private:
	SpdLogger logger;
};

} // namespace villas
} // namespace gpu
