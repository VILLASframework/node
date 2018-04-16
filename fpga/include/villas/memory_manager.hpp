#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <unistd.h>

#include "log.hpp"
#include "directed_graph.hpp"

namespace villas {

/**
 * @brief Translation between a local (master) to a foreign (slave) address space
 *
 * Memory translations can be chained together using the `+=` operator which is
 * used internally by the MemoryManager to compute a translation through
 * multiple hops (memory mappings).
 */
class MemoryTranslation {
public:

	/**
	 * @brief MemoryTranslation
	 * @param src	Base address of local address space
	 * @param dst	Base address of foreign address space
	 * @param size	Size of "memory window"
	 */
	MemoryTranslation(uintptr_t src, uintptr_t dst, size_t size) :
	    src(src), dst(dst), size(size) {}

	uintptr_t
	getLocalAddr(uintptr_t addrInForeignAddrSpace) const;

	uintptr_t
	getForeignAddr(uintptr_t addrInLocalAddrSpace) const;

	size_t
	getSize() const
	{ return size; }

	friend std::ostream&
	operator<< (std::ostream& stream, const MemoryTranslation& translation)
	{
		return stream << std::hex
		              << "(src=0x"   << translation.src
		              << ", dst=0x"  << translation.dst
		              << ", size=0x" << translation.size
		              << ")";
	}

	/// Merge two MemoryTranslations together
	MemoryTranslation& operator+=(const MemoryTranslation& other);

private:
	uintptr_t	src;	///< Base address of local address space
	uintptr_t	dst;	///< Base address of foreign address space
	size_t		size;	///< Size of "memory window"
};


/**
 * @brief Global memory manager to resolve addresses across address spaces
 *
 * Every entity in the system has to register its (master) address space and
 * create mappings to other (slave) address spaces that it can access. A
 * directed graph is then constructed which allows to traverse addresses spaces
 * through multiple mappings and resolve addresses through this "tunnel" of
 * memory mappings.
 */
class MemoryManager {
private:
	// This is a singleton, so private constructor ...
	MemoryManager() :
	    memoryGraph("MemoryGraph"),
	    logger(loggerGetOrCreate("MemoryManager")) {}

	// ... and no copying or assigning
	MemoryManager(const MemoryManager&) = delete;
	MemoryManager& operator=(const MemoryManager&) = delete;

	/**
	 * @brief Custom edge in memory graph representing a memory mapping
	 *
	 * A memory mapping maps from one address space into another and can only be
	 * traversed in the forward direction which reflects the nature of real
	 * memory mappings.
	 *
	 * Implementation Notes:
	 * The member #src is the address in the "from" address space, where the
	 * destination address space is mapped. The member #dest is the address in
	 * the destination address space, where the mapping points to. Often, #dest
	 * will be zero for mappings to hardware, but consider the example when
	 * mapping FPGA to application memory:
	 * The application allocates a block 1kB at address 0x843001000 in its
	 * address space. The mapping would then have a #dest address of 0x843001000
	 * and a #size of 1024.
	 */
	class Mapping : public graph::Edge {
	public:
		std::string	name;	///< Human-readable name
		uintptr_t	src;	///< Base address in "from" address space
		uintptr_t	dest;	///< Base address in "to" address space
		size_t		size;	///< Size of the mapping

		friend std::ostream&
		operator<< (std::ostream& stream, const Mapping& mapping)
		{
			return stream << static_cast<const Edge&>(mapping) << " = "
			              << mapping.name
			              << std::hex
			              << "(src=0x"   << mapping.src
			              << ", dest=0x" << mapping.dest
			              << ", size=0x" << mapping.size
			              << ")";
		}

	};


	/**
	 * @brief Custom vertex in memory graph representing an address space
	 *
	 * Since most information in the memory graph is stored in the edges (memory
	 * mappings), this is just a small extension to the default vertex. It only
	 * associates an additional string #name for human-readability.
	 */
	class AddressSpace : public graph::Vertex {
	public:
		std::string name;	///< Human-readable name

		friend std::ostream&
		operator<< (std::ostream& stream, const AddressSpace& addrSpace)
		{
			return stream << static_cast<const Vertex&>(addrSpace) << " = "
			              << addrSpace.name;
		}
	};

	/// Memory graph with custom edges and vertices for address resolution
	using MemoryGraph = graph::DirectedGraph<AddressSpace, Mapping>;

public:
	using AddressSpaceId = MemoryGraph::VertexIdentifier;
	using MappingId = MemoryGraph::EdgeIdentifier;

	/// Get singleton instance
	static MemoryManager&
	get();

	AddressSpaceId
	getProcessAddressSpace()
	{ return getOrCreateAddressSpace("villas-fpga"); }

	AddressSpaceId
	getProcessAddressSpaceMemoryBlock(const std::string& memoryBlock)
	{ return getOrCreateAddressSpace(getSlaveAddrSpaceName("villas-fpga", memoryBlock)); }


	AddressSpaceId
	getOrCreateAddressSpace(std::string name);

	void
	removeAddressSpace(AddressSpaceId addrSpaceId)
	{ memoryGraph.removeVertex(addrSpaceId); }

	/// Create a default mapping
	MappingId
	createMapping(uintptr_t src, uintptr_t dest, size_t size,
	              const std::string& name,
	              AddressSpaceId fromAddrSpace,
	              AddressSpaceId toAddrSpace);

	/// Add a mapping
	///
	/// Can be used to derive from Mapping in order to implement custom
	/// constructor/destructor.
	MappingId
	addMapping(std::shared_ptr<Mapping> mapping,
	           AddressSpaceId fromAddrSpace,
	           AddressSpaceId toAddrSpace);


	AddressSpaceId
	findAddressSpace(const std::string& name);

	MemoryTranslation
	getTranslation(AddressSpaceId fromAddrSpaceId, AddressSpaceId toAddrSpaceId);

	MemoryTranslation
	getTranslationFromProcess(AddressSpaceId foreignAddrSpaceId)
	{ return getTranslation(getProcessAddressSpace(), foreignAddrSpaceId); }

	static std::string
	getSlaveAddrSpaceName(const std::string& ipInstance, const std::string& memoryBlock)
	{ return ipInstance + "/" + memoryBlock; }

	static std::string
	getMasterAddrSpaceName(const std::string& ipInstance, const std::string& busInterface)
	{ return ipInstance + ":" + busInterface; }

	void
	dump()
	{ memoryGraph.dump(); }


private:
	/// Convert a Mapping to MemoryTranslation for calculations
	static MemoryTranslation
	getTranslationFromMapping(const Mapping& mapping)
	{ return MemoryTranslation(mapping.src, mapping.dest, mapping.size); }


private:
	/// Directed graph that stores address spaces and memory mappings
	MemoryGraph memoryGraph;

	/// Cache mapping of names to address space ids for fast lookup
	std::map<std::string, AddressSpaceId> addrSpaceLookup;

	/// Logger for universal access in this class
	SpdLogger logger;

	/// Static pointer to global instance, because this is a singleton
	static MemoryManager* instance;
};

} // namespace villas
