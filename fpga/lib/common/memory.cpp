#include <sys/mman.h>
#include <unistd.h>

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
    internalOffset(internalOffset)
{
	// make sure to start at aligned offset, reduce size in case we need padding
	if(const size_t paddingBytes = getAlignmentPadding(internalOffset)) {
		assert(paddingBytes < memorySize);

		internalOffset += paddingBytes;
		memorySize -= paddingBytes;
	}

	// deallocation callback
	free = [&](MemoryBlock* mem) {
		logger->debug("freeing {:#x} bytes at local addr {:#x} (addr space {})",
		              mem->getSize(), mem->getOffset(), mem->getAddrSpaceId());
		logger->warn("free() not implemented");
		logger->debug("available memory: {:#x} bytes", getAvailableMemory());

		removeMemoryBlock(*mem);
	};
}


std::string
LinearAllocator::getName() const
{
	std::stringstream name;
	name << "LinearAlloc" << getAddrSpaceId()
	     << "@0x" << std::hex << internalOffset;
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

} // namespace villas
