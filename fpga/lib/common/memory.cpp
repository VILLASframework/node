#include <sys/mman.h>
#include <unistd.h>

#include <sstream>
#include <fstream>

#include "memory.hpp"

namespace villas {

std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
HostRam::HostRamAllocator::allocateBlock(size_t size)
{
	/* Align to next bigger page size chunk */
	if (size &  size_t(0xFFF)) {
		size += size_t(0x1000);
		size &= size_t(~0xFFF);
	}

	const int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT;
	const int mmap_protection = PROT_READ | PROT_WRITE;

	const void* addr = mmap(nullptr, size, mmap_protection, mmap_flags, 0, 0);
	if(addr == nullptr) {
		throw std::bad_alloc();
	}

	auto& mm = MemoryManager::get();

	// assemble name for this block
	std::stringstream name;
	name << std::showbase << std::hex << reinterpret_cast<uintptr_t>(addr);

	auto blockAddrSpaceId = mm.getProcessAddressSpaceMemoryBlock(name.str());

	const auto localAddr = reinterpret_cast<uintptr_t>(addr);
	std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
	        mem(new MemoryBlock(localAddr, size, blockAddrSpaceId), this->free);

	insertMemoryBlock(*mem);

	return mem;
}


LinearAllocator::LinearAllocator(MemoryManager::AddressSpaceId memoryAddrSpaceId,
                                 size_t memorySize,
                                 size_t internalOffset) :
    BaseAllocator(memoryAddrSpaceId),
    nextFreeAddress(0),
    memorySize(memorySize),
    internalOffset(internalOffset),
    allocationCount(0)
{
	// make sure to start at aligned offset, reduce size in case we need padding
	if(const size_t paddingBytes = getAlignmentPadding(internalOffset)) {
		assert(paddingBytes < memorySize);

		internalOffset += paddingBytes;
		memorySize -= paddingBytes;
	}

	// deallocation callback
	free = [&](MemoryBlock* mem) {
		logger->debug("Deallocating memory block at local addr {:#x} (addr space {})",
		              mem->getOffset(), mem->getAddrSpaceId());

		removeMemoryBlock(*mem);

		allocationCount--;
		if(allocationCount == 0) {
			logger->debug("All allocations are deallocated now, freeing memory");

			// all allocations have been deallocated, free all memory
			nextFreeAddress = 0;
		}

		logger->debug("Available memory: {:#x} bytes", getAvailableMemory());
	};
}


std::string
LinearAllocator::getName() const
{
	std::stringstream name;
	name << "LinearAlloc" << getAddrSpaceId();
	if(internalOffset != 0) {
		name << "@0x" << std::hex << internalOffset;
	}

	return name.str();
}


std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
LinearAllocator::allocateBlock(size_t size)
{
	if(size > getAvailableMemory()) {
		throw std::bad_alloc();
	}

	// assign address
	const uintptr_t localAddr = nextFreeAddress + internalOffset;

	// reserve memory
	nextFreeAddress += size;

	// make sure it is aligned
	if(const size_t paddingBytes = getAlignmentPadding(nextFreeAddress)) {
		nextFreeAddress += paddingBytes;

		// if next free address is outside this block due to padding, cap it
		nextFreeAddress = std::min(nextFreeAddress, memorySize);
	}


	auto& mm = MemoryManager::get();

	// assemble name for this block
	std::stringstream blockName;
	blockName << std::showbase << std::hex << localAddr;

	// create address space
	auto addrSpaceName = mm.getSlaveAddrSpaceName(getName(), blockName.str());
	auto addrSpaceId = mm.getOrCreateAddressSpace(addrSpaceName);

	logger->debug("Allocated {:#x} bytes for {}, {:#x} bytes remaining",
	              size, addrSpaceId, getAvailableMemory());

	std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
	        mem(new MemoryBlock(localAddr, size, addrSpaceId), this->free);

	// mount block into the memory graph
	insertMemoryBlock(*mem);

	// increase the allocation count
	allocationCount++;

	return mem;
}


HostRam::HostRamAllocator
HostRam::allocator;

HostRam::HostRamAllocator::HostRamAllocator() :
    BaseAllocator(MemoryManager::get().getProcessAddressSpace())
{
	free = [&](MemoryBlock* mem) {
		if(::munmap(reinterpret_cast<void*>(mem->getOffset()), mem->getSize()) != 0) {
			logger->warn("munmap() failed for {:#x} of size {:#x}",
			             mem->getOffset(), mem->getSize());
		}

		removeMemoryBlock(*mem);
	};
}


std::map<int, std::unique_ptr<HostDmaRam::HostDmaRamAllocator>>
HostDmaRam::allocators;

HostDmaRam::HostDmaRamAllocator::HostDmaRamAllocator(int num) :
    LinearAllocator(MemoryManager::get().getOrCreateAddressSpace(getUdmaBufName(num)), getUdmaBufBufSize(num)),
    num(num)
{
	auto& mm = MemoryManager::get();
	logger = loggerGetOrCreate(getName());

	if(getSize() == 0) {
		logger->error("Zero-sized DMA buffer not supported, is the kernel module loaded?");
		throw std::bad_alloc();
	}

	const uintptr_t base = getUdmaBufPhysAddr(num);

	mm.createMapping(base, 0, getSize(), getName() + "-PCI",
	                 mm.getPciAddressSpace(), getAddrSpaceId());

	const auto bufPath = std::string("/dev/") + getUdmaBufName(num);
	const int bufFd = open(bufPath.c_str(), O_RDWR | O_SYNC);
	if(bufFd != -1) {
		void* buf = mmap(nullptr, getSize(), PROT_READ|PROT_WRITE, MAP_SHARED, bufFd, 0);
		close(bufFd);

		if(buf != MAP_FAILED) {
			mm.createMapping(reinterpret_cast<uintptr_t>(buf), 0, getSize(),
			                 getName() + "-VA",
			                 mm.getProcessAddressSpace(), getAddrSpaceId());
		} else {
			logger->warn("Cannot map {}", bufPath);
		}
	} else {
		logger->warn("Cannot open {}", bufPath);
	}

	logger->info("Mapped {} of size {} bytes", bufPath, getSize());
}

HostDmaRam::HostDmaRamAllocator::~HostDmaRamAllocator()
{
	auto& mm = MemoryManager::get();

	void* baseVirt;
	try {
		auto translation = mm.getTranslationFromProcess(getAddrSpaceId());
		baseVirt = reinterpret_cast<void*>(translation.getLocalAddr(0));
	} catch(const std::out_of_range&) {
		// not mapped, nothing to do
		return;
	}

	logger->debug("Unmapping {}", getName());

	// try to unmap it
	if(::munmap(baseVirt, getSize()) != 0) {
		logger->warn("munmap() failed for {:p} of size {:#x}",
		             baseVirt, getSize());
	}
}

std::string
HostDmaRam::getUdmaBufName(int num)
{
	std::stringstream name;
	name << "udmabuf" << num;

	return name.str();
}

std::string
HostDmaRam::getUdmaBufBasePath(int num)
{
	std::stringstream path;
	path << "/sys/class/udmabuf/udmabuf" << num << "/";
	return path.str();
}

size_t
HostDmaRam::getUdmaBufBufSize(int num)
{
	std::fstream s(getUdmaBufBasePath(num) + "size", s.in);
	if(s.is_open()) {
		std::string line;
		if(std::getline(s, line)) {
			return std::strtoul(line.c_str(), nullptr, 10);
		}
	}

	return 0;
}

uintptr_t
HostDmaRam::getUdmaBufPhysAddr(int num)
{
	std::fstream s(getUdmaBufBasePath(num) + "phys_addr", s.in);
	if(s.is_open()) {
		std::string line;
		if(std::getline(s, line)) {
			return std::strtoul(line.c_str(), nullptr, 16);
		}
	}

	return UINTPTR_MAX;
}

HostDmaRam::HostDmaRamAllocator&HostDmaRam::getAllocator(int num)
{
	auto& allocator = allocators[num];
	if(not allocator) {
		allocator = std::make_unique<HostDmaRamAllocator>(num);
	}

	return *allocator;
}

} // namespace villas
