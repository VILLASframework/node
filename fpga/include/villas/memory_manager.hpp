#pragma once

#include <cstdint>
#include <string>

#include "log.hpp"
#include "directed_graph.hpp"

namespace villas {


class Mapping : public graph::Edge {
	friend class MemoryManager;

public:
	// create mapping here (if needed)
	Mapping() {}

	// destroy mapping here (if needed)
	virtual ~Mapping();

	friend std::ostream&
	operator<< (std::ostream& stream, const Mapping& mapping)
	{
		return stream << static_cast<const Edge&>(mapping) << " = "
		              << std::hex
		              << "(src=0x"   << mapping.src
		              << ", dest=0x" << mapping.dest
		              << ", size=0x" << mapping.size
		              << ")";
	}

private:
    uintptr_t src;
    uintptr_t dest;
    size_t size;
};

class AddressSpace : public graph::Vertex {
	friend class MemoryManager;

public:
	friend std::ostream&
	operator<< (std::ostream& stream, const AddressSpace& addrSpace)
	{
		return stream << static_cast<const Vertex&>(addrSpace) << " = "
		              << addrSpace.name;
	}

private:
	std::string name;
};


// is or has a graph
class MemoryManager {
private:
	// This is a singleton, so private constructor
	MemoryManager() :
	    memoryGraph("MemoryGraph") {}

	// no copying or assigning
	MemoryManager(const MemoryManager&) = delete;
	MemoryManager& operator=(const MemoryManager&) = delete ;

	using MemoryGraph = graph::DirectedGraph<AddressSpace, Mapping>;

public:
	using AddressSpaceId = MemoryGraph::VertexIdentifier;
	using MappingId = MemoryGraph::EdgeIdentifier;

	static MemoryManager& get();


	AddressSpaceId createAddressSpace(std::string name);

	/// Create a default mapping
	MappingId createMapping(uintptr_t src, uintptr_t dest, size_t size,
	                        AddressSpaceId fromAddrSpace,
	                        AddressSpaceId toAddrSpace);

	/// Add a mapping
	///
	/// Can be used to derive from Mapping in order to implement custom
	/// constructor/destructor.
	MappingId addMapping(std::shared_ptr<Mapping> mapping,
	                     AddressSpaceId fromAddrSpace,
	                     AddressSpaceId toAddrSpace);

	void dump()
	{ memoryGraph.dump(); }

private:
	MemoryGraph memoryGraph;
	static MemoryManager* instance;
};

} // namespace villas
