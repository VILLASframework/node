#pragma once

#include <string>
#include <unistd.h>

#include "log.hpp"
#include "memory_manager.hpp"

namespace villas {

class MemoryBlock {
protected:
	MemoryBlock(MemoryManager::AddressSpaceId addrSpaceId, size_t size) :
	    addrSpaceId(addrSpaceId), size(size) {}

public:
	MemoryManager::AddressSpaceId getAddrSpaceId() const
	{ return addrSpaceId; }

	size_t getSize() const
	{ return size; }

private:
	MemoryManager::AddressSpaceId addrSpaceId;
	size_t size;
};


class MemoryAllocator {
};


class HostRam : public MemoryAllocator {
public:

	template<typename T>
	class MemoryBlockHostRam : public MemoryBlock {
		friend class HostRam;
	private:
		MemoryBlockHostRam(void* addr, size_t size, MemoryManager::AddressSpaceId foreignAddrSpaceId) :
		    MemoryBlock(foreignAddrSpaceId, size),
		    addr(addr),
		    translation(MemoryManager::get().getTranslationFromProcess(foreignAddrSpaceId))
		{}
	public:
		using Type = T;

		MemoryBlockHostRam() = delete;

		T& operator*() {
			return *reinterpret_cast<T*>(translation.getLocalAddr(0));
		}

		T& operator [](int idx) {
			const size_t offset = sizeof(T) * idx;
			return *reinterpret_cast<T*>(translation.getLocalAddr(offset));
		}

		T* operator &() const {
			return reinterpret_cast<T*>(translation.getLocalAddr(0));
		}

	private:
		// addr needed for freeing later
		void* addr;

		// cached memory translation for fast access
		MemoryTranslation translation;
	};

	template<typename T>
	static MemoryBlockHostRam<T>
	allocate(size_t num)
	{
		/* Align to next bigger page size chunk */
		size_t length = num * sizeof(T);
		if (length &  size_t(0xFFF)) {
			length += size_t(0x1000);
			length &= size_t(~0xFFF);
		}

		void* const addr = HostRam::allocate(length);
		if(addr == nullptr) {
			throw std::bad_alloc();
		}

		auto& mm = MemoryManager::get();

		// assemble name for this block
		std::stringstream name;
		name << std::showbase << std::hex << reinterpret_cast<uintptr_t>(addr);

		auto blockAddrSpaceId = mm.getProcessAddressSpaceMemoryBlock(name.str());

		// create mapping from VA space of process to this new block
		mm.createMapping(reinterpret_cast<uintptr_t>(addr), 0, length,
		                 "VA",
		                 mm.getProcessAddressSpace(),
		                 blockAddrSpaceId);

		// create object and corresponding address space in memory manager
		return MemoryBlockHostRam<T>(addr, length, blockAddrSpaceId);
	}

	template<typename T>
	static inline bool
	free(const MemoryBlockHostRam<T>& block)
	{
		// TODO: remove address space from memory manager
		// TODO: how to prevent use after free?
		return HostRam::free(block.addr, block.size);
	}

private:
	static void*
	allocate(size_t length, int flags = 0);

	static bool
	free(void*, size_t length);
};

} // namespace villas
