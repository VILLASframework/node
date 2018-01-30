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


} // namespace villas
