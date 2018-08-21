/** GPU managment.
 *
 * @file
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

#pragma once

#include <sstream>

#include <villas/plugin.hpp>
#include <villas/memory_manager.hpp>
#include <villas/memory.hpp>
#include <villas/log.hpp>

namespace villas {
namespace gpu {

class GpuAllocator;

class Gpu {
	friend GpuAllocator;
public:
	Gpu(int gpuId);
	~Gpu();

	bool init();

	std::string getName() const;

	GpuAllocator& getAllocator() const
	{ return *allocator; }


	bool makeAccessibleToPCIeAndVA(const MemoryBlock& mem);

	/// Make some memory block accssible for this GPU
	bool makeAccessibleFromPCIeOrHostRam(const MemoryBlock& mem);

	void memcpySync(const MemoryBlock& src, const MemoryBlock& dst, size_t size);

	void memcpyKernel(const MemoryBlock& src, const MemoryBlock& dst, size_t size);

private:
	bool registerIoMemory(const MemoryBlock& mem);
	bool registerHostMemory(const MemoryBlock& mem);

private:
	class impl;
	std::unique_ptr<impl> pImpl;

	// master, will be used to derived slave addr spaces for allocation
	MemoryManager::AddressSpaceId masterPciEAddrSpaceId;

	MemoryManager::AddressSpaceId slaveMemoryAddrSpaceId;

	SpdLogger logger;

	int gpuId;

	std::unique_ptr<GpuAllocator> allocator;
};


class GpuAllocator : public BaseAllocator<GpuAllocator> {
public:
	GpuAllocator(Gpu& gpu);

	std::string getName() const;

	std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
	allocateBlock(size_t size);

private:
	Gpu& gpu;
};

class GpuFactory : public Plugin {
public:
	GpuFactory();

	std::list<std::unique_ptr<Gpu>>
	make();

	void run(void*);

private:
	SpdLogger logger;
};

} // namespace villas
} // namespace gpu
