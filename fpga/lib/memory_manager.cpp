#include <memory>

#include "memory_manager.hpp"

namespace villas {

MemoryManager*
MemoryManager::instance = nullptr;

MemoryManager&
MemoryManager::get()
{
	if(instance == nullptr) {
		instance = new MemoryManager;
	}

	return *instance;
}

MemoryManager::AddressSpaceId
MemoryManager::createAddressSpace(std::string name)
{
	std::shared_ptr<AddressSpace> addrSpace(new AddressSpace);
	addrSpace->name = name;

	return memoryGraph.addVertex(addrSpace);
}

MemoryManager::MappingId
MemoryManager::createMapping(uintptr_t src, uintptr_t dest, size_t size,
                             MemoryManager::AddressSpaceId fromAddrSpace,
                             MemoryManager::AddressSpaceId toAddrSpace)
{
	std::shared_ptr<Mapping> mapping(new Mapping);
	mapping->src = src;
	mapping->dest = dest;
	mapping->size = size;

	return addMapping(mapping, fromAddrSpace, toAddrSpace);
}

MemoryManager::MappingId
MemoryManager::addMapping(std::shared_ptr<Mapping> mapping,
                          MemoryManager::AddressSpaceId fromAddrSpace,
                          MemoryManager::AddressSpaceId toAddrSpace)
{
	return memoryGraph.addEdge(mapping, fromAddrSpace, toAddrSpace);
}


Mapping::~Mapping()
{

}


} // namespace villas
