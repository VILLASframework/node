#pragma once

#include <cstdint>

#include "log.hpp"
#include "directed_graph.hpp"

namespace villas {


class Mapping : public graph::Edge {
public:
    // create mapping here (if needed)
    Mapping() {}

    // destroy mapping here (if needed)
    ~Mapping() {}

private:
    uintptr_t src;
    uintptr_t dest;
    size_t size;
};

class AddressSpace : public graph::Vertex {
private:
    // do we need any metadata? maybe a name?
    int id;
};


// is or has a graph
class MemoryManager {

// This is a singleton
private:
    MemoryManager() = default;
    static MemoryManager* instance;
public:
    static MemoryManager& get();


private:
//    std::list<>
};

} // namespace villas
