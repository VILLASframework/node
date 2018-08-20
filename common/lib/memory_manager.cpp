/** Memory managment.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <memory>
#include <limits>
#include <cstdint>

#include <villas/utils.hpp>
#include <villas/memory_manager.hpp>

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

std::list<MemoryManager::AddressSpaceId>
MemoryManager::findPath(MemoryManager::AddressSpaceId fromAddrSpaceId,
                        MemoryManager::AddressSpaceId toAddrSpaceId)
{
	std::list<AddressSpaceId> path;

	auto fromAddrSpace = memoryGraph.getVertex(fromAddrSpaceId);
	auto toAddrSpace = memoryGraph.getVertex(toAddrSpaceId);

	// find a path through the memory graph
	MemoryGraph::Path pathGraph;
	if(not memoryGraph.getPath(fromAddrSpaceId, toAddrSpaceId, pathGraph, pathCheckFunc)) {

		logger->debug("No translation found from ({}) to ({})",
		              *fromAddrSpace, *toAddrSpace);

		throw std::out_of_range("no translation found");
	}

	for(auto& mappingId : pathGraph) {
		auto mapping = memoryGraph.getEdge(mappingId);
		path.push_back(mapping->getVertexTo());
	}

	return path;
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
		logger->debug("No translation found from ({}) to ({})",
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

	logger->debug("this->src:      {:#x}", this->src);
	logger->debug("this->dst:      {:#x}", this->dst);
	logger->debug("this->size:     {:#x}", this->size);
	logger->debug("other.src:      {:#x}", other.src);
	logger->debug("other.dst:      {:#x}", other.dst);
	logger->debug("other.size:     {:#x}", other.size);
	logger->debug("this_dst_high:  {:#x}", this_dst_high);
	logger->debug("other_src_high: {:#x}", other_src_high);

	// make sure there is a common memory area
	assertExcept(other.src < this_dst_high, MemoryManager::InvalidTranslation());
	assertExcept(this->dst < other_src_high, MemoryManager::InvalidTranslation());

	const uintptr_t hi = std::max(this_dst_high, other_src_high);
	const uintptr_t lo = std::min(this->dst, other.src);

	const uintptr_t diff_hi = (this_dst_high > other_src_high)
	                          ? (this_dst_high - other_src_high)
	                          : (other_src_high - this_dst_high);

	const bool otherSrcIsSmaller = this->dst > other.src;
	const uintptr_t diff_lo = (otherSrcIsSmaller)
	                          ? (this->dst - other.src)
	                          : (other.src - this->dst);

	logger->debug("hi:             {:#x}", hi);
	logger->debug("lo:             {:#x}", lo);
	logger->debug("diff_hi:        {:#x}", diff_hi);
	logger->debug("diff_lo:        {:#x}", diff_lo);

	// new size of aperture, can only stay or shrink
	this->size = (hi - lo) - diff_hi - diff_lo;

	// new translation will come out other's destination (by default)
	this->dst = other.dst;

	// the source stays the same and can only increase with merged translations
	this->src = this->src;

	if(otherSrcIsSmaller) {
		// other mapping starts at lower addresses, so we actually arrive at
		// higher addresses
		this->dst += diff_lo;
	} else {
		// other mapping starts at higher addresses than this, so we have to
		// increase the start
		// NOTE: for addresses equality, this just adds 0
		this->src += diff_lo;
	}

	logger->debug("result src:     {:#x}", this->src);
	logger->debug("result dst:     {:#x}", this->dst);
	logger->debug("result size:    {:#x}", this->size);

	return *this;
}

} // namespace villas
