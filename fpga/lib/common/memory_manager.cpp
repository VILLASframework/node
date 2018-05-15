#include <memory>
#include <limits>
#include <cstdint>

#include <villas/utils.hpp>
#include "memory_manager.hpp"

using namespace villas::utils;

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
MemoryManager::getOrCreateAddressSpace(std::string name)
{
	try {
		// try fast lookup
		return addrSpaceLookup.at(name);
	} catch (const std::out_of_range&) {
		// does not yet exist, create
		std::shared_ptr<AddressSpace> addrSpace(new AddressSpace);
		addrSpace->name = name;

		// cache it for the next access
		addrSpaceLookup[name] = memoryGraph.addVertex(addrSpace);

		return addrSpaceLookup[name];
	}
}

MemoryManager::MappingId
MemoryManager::createMapping(uintptr_t src, uintptr_t dest, size_t size,
                             const std::string& name,
                             MemoryManager::AddressSpaceId fromAddrSpace,
                             MemoryManager::AddressSpaceId toAddrSpace)
{
	std::shared_ptr<Mapping> mapping(new Mapping);

	mapping->name = name;
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

MemoryManager::AddressSpaceId
MemoryManager::findAddressSpace(const std::string& name)
{
	return memoryGraph.findVertex(
	            [&](const std::shared_ptr<AddressSpace>& v) {
		return v->name == name;
	});
}

MemoryTranslation
MemoryManager::getTranslation(MemoryManager::AddressSpaceId fromAddrSpaceId,
                              MemoryManager::AddressSpaceId toAddrSpaceId)
{
	// find a path through the memory graph
	MemoryGraph::Path path;
	if(not memoryGraph.getPath(fromAddrSpaceId, toAddrSpaceId, path, pathCheckFunc)) {

		auto fromAddrSpace = memoryGraph.getVertex(fromAddrSpaceId);
		auto toAddrSpace = memoryGraph.getVertex(toAddrSpaceId);

		logger->error("No translation found from ({}) to ({})",
		              *fromAddrSpace, *toAddrSpace);

		throw std::out_of_range("no translation found");
	}

	// start with an identity mapping
	MemoryTranslation translation(0, 0, SIZE_MAX);

	// iterate through path and merge all mappings into a single translation
	for(auto& mappingId : path) {
		auto mapping = memoryGraph.getEdge(mappingId);
		translation += getTranslationFromMapping(*mapping);
	}

	return translation;
}

bool
MemoryManager::pathCheck(const MemoryGraph::Path& path)
{
	// start with an identity mapping
	MemoryTranslation translation(0, 0, SIZE_MAX);

	// Try to add all mappings together to a common translation. If this fails
	// there is a non-overlapping window
	for(auto& mappingId : path) {
		auto mapping = memoryGraph.getEdge(mappingId);
		try {
			translation += getTranslationFromMapping(*mapping);
		} catch(const InvalidTranslation&) {
			return false;
		}
	}

	return true;
}

uintptr_t
MemoryTranslation::getLocalAddr(uintptr_t addrInForeignAddrSpace) const
{
	assert(addrInForeignAddrSpace >= dst);
	assert(addrInForeignAddrSpace < (dst + size));
	return src + addrInForeignAddrSpace - dst;
}

uintptr_t
MemoryTranslation::getForeignAddr(uintptr_t addrInLocalAddrSpace) const
{
	assert(addrInLocalAddrSpace >= src);
	assert(addrInLocalAddrSpace < (src + size));
	return dst + addrInLocalAddrSpace - src;
}

MemoryTranslation&
MemoryTranslation::operator+=(const MemoryTranslation& other)
{
	auto logger = loggerGetOrCreate("MemoryTranslation");
	// set level to debug to enable debug output
	logger->set_level(spdlog::level::info);

	const uintptr_t this_dst_high = this->dst + this->size;
	const uintptr_t other_src_high = other.src + other.size;

	// make sure there is a common memory area
	assertExcept(other.src < this_dst_high, MemoryManager::InvalidTranslation());
	assertExcept(this->dst < other_src_high, MemoryManager::InvalidTranslation());

	const uintptr_t hi = std::max(this_dst_high, other_src_high);
	const uintptr_t lo = std::min(this->dst, other.src);

	const uintptr_t diff_hi = (this_dst_high > other_src_high)
	                          ? (this_dst_high - other_src_high)
	                          : (other_src_high - this_dst_high);

	const uintptr_t diff_lo = (this->dst > other.src)
	                          ? (this->dst - other.src)
	                          : (other.src - this->dst);

	const size_t size = (hi - lo) - diff_hi - diff_lo;

	logger->debug("this->src:      0x{:x}", this->src);
	logger->debug("this->dst:      0x{:x}", this->dst);
	logger->debug("this->size:     0x{:x}", this->size);
	logger->debug("other.src:      0x{:x}", other.src);
	logger->debug("other.dst:      0x{:x}", other.dst);
	logger->debug("other.size:     0x{:x}", other.size);
	logger->debug("this_dst_high:  0x{:x}", this_dst_high);
	logger->debug("other_src_high: 0x{:x}", other_src_high);
	logger->debug("hi:             0x{:x}", hi);
	logger->debug("lo:             0x{:x}", lo);
	logger->debug("diff_hi:        0x{:x}", diff_hi);
	logger->debug("diff_hi:        0x{:x}", diff_lo);
	logger->debug("size:           0x{:x}", size);

	this->src += other.src;
	this->dst += other.dst;
	this->size = size;

	logger->debug("result src:     0x{:x}", this->src);
	logger->debug("result dst:     0x{:x}", this->dst);
	logger->debug("result size:    0x{:x}", this->size);

	return *this;
}

} // namespace villas
