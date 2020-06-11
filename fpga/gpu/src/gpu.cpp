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

#include <cstdio>
#include <cstdint>
#include <sys/mman.h>

#include <memory>
#include <algorithm>

#include <villas/gpu.hpp>
#include <villas/log.hpp>
#include <villas/kernel/pci.h>
#include <villas/memory_manager.hpp>

#include <cuda.h>
#include <cuda_runtime.h>
#include <gdrapi.h>

#include "kernels.hpp"

namespace villas {
namespace gpu {

static GpuFactory gpuFactory;

GpuAllocator::GpuAllocator(Gpu& gpu) :
    BaseAllocator(gpu.masterPciEAddrSpaceId),
    gpu(gpu)
{
	free = [&](MemoryBlock* mem) {
		cudaSetDevice(gpu.gpuId);
		if (cudaFree(reinterpret_cast<void*>(mem->getOffset())) != cudaSuccess) {
			logger->warn("cudaFree() failed for {:#x} of size {:#x}",
			             mem->getOffset(), mem->getSize());
		}

		removeMemoryBlock(*mem);
	};
}

std::string
villas::gpu::GpuAllocator::getName() const
{
	std::stringstream name;
	name << "GpuAlloc" << getAddrSpaceId();
	return name.str();
}


GpuFactory::GpuFactory() :
    Plugin("cuda", "CUDA capable GPUs")
{
	logger = villas::logging.get("GpuFactory");
}

// required to be defined here for PIMPL to compile
Gpu::~Gpu()
{
	auto& mm = MemoryManager::get();
	mm.removeAddressSpace(masterPciEAddrSpaceId);
}


// we use PIMPL in order to hide gdrcopy types from the public header
class Gpu::impl {
public:
	gdr_t gdr;
	struct pci_device pdev;
};

std::string Gpu::getName() const
{
	cudaDeviceProp deviceProp;
	if (cudaGetDeviceProperties(&deviceProp, gpuId) != cudaSuccess) {
		// logger not yet availabe
		villas::logging.get("Gpu")->error("Cannot retrieve properties for GPU {}", gpuId);
		throw std::exception();
	}

	std::stringstream name;
	name << "gpu" << gpuId << "(" << deviceProp.name << ")";

	return name.str();
}

bool Gpu::registerIoMemory(const MemoryBlock& mem)
{
	auto& mm = MemoryManager::get();
	const auto pciAddrSpaceId = mm.getPciAddressSpace();

	// Check if we need to map anything at all, maybe it's already reachable
	try {
		// TODO: there might already be a path through the graph, but there's no
		// overlapping window, so this will fail badly!
		auto translation = mm.getTranslation(masterPciEAddrSpaceId,
		                                     mem.getAddrSpaceId());
		if (translation.getSize() >= mem.getSize()) {
			// there is already a sufficient path
			logger->debug("Already mapped through another mapping");
			return true;
		} else {
			logger->warn("There's already a mapping, but too small");
		}
	} catch(const std::out_of_range&) {
		// not yet reachable, that's okay, proceed
	}


	// In order to register IO memory with CUDA, it has to be mapped to the VA
	// space of the current process (requirement of CUDA API). Check this now.
	MemoryManager::AddressSpaceId mappedBaseAddrSpaceId;
	try {
		auto path = mm.findPath(mm.getProcessAddressSpace(), mem.getAddrSpaceId());
		// first node in path is the mapped memory space whose virtual address
		// we need to hand to CUDA
		mappedBaseAddrSpaceId = path.front();
	} catch (const std::out_of_range&) {
		logger->error("Memory not reachable from process, but required by CUDA");
		return false;
	}

	// determine the base address of the mapped memory region needed by CUDA
	const auto translationProcess = mm.getTranslationFromProcess(mappedBaseAddrSpaceId);
	const uintptr_t baseAddrForProcess = translationProcess.getLocalAddr(0);


	// Now check that the memory is also reachable via PCIe bus, otherwise GPU
	// has no means to access it.
	uintptr_t baseAddrOnPci;
	size_t	sizeOnPci;
	try {
		auto translationPci = mm.getTranslation(pciAddrSpaceId,
		                                        mappedBaseAddrSpaceId);
		baseAddrOnPci = translationPci.getLocalAddr(0);
		sizeOnPci = translationPci.getSize();
	} catch(const std::out_of_range&) {
		logger->error("Memory is not reachable via PCIe bus");
		return false;
	}

	if (sizeOnPci < mem.getSize()) {
		logger->warn("VA mapping of IO memory is too small: {:#x} instead of {:#x} bytes",
		              sizeOnPci, mem.getSize());
		logger->warn("If something later on fails or behaves strangely, this might be the cause!");
	}


	cudaSetDevice(gpuId);

	auto baseAddrVA = reinterpret_cast<void*>(baseAddrForProcess);
	if (cudaHostRegister(baseAddrVA, sizeOnPci, cudaHostRegisterIoMemory) != cudaSuccess) {
		logger->error("Cannot register IO memory for block {}", mem.getAddrSpaceId());
		return false;
	}

	void* devicePointer = nullptr;
	if (cudaHostGetDevicePointer(&devicePointer, baseAddrVA, 0) != cudaSuccess) {
		logger->error("Cannot retrieve device pointer for IO memory");
		return false;
	}

	mm.createMapping(reinterpret_cast<uintptr_t>(devicePointer), baseAddrOnPci,
	                 sizeOnPci, "CudaIoMem", masterPciEAddrSpaceId, pciAddrSpaceId);

	return true;
}

bool
Gpu::registerHostMemory(const MemoryBlock& mem)
{
	auto& mm = MemoryManager::get();

	auto translation = mm.getTranslationFromProcess(mem.getAddrSpaceId());
	auto localBase = reinterpret_cast<void*>(translation.getLocalAddr(0));

	int ret = cudaHostRegister(localBase, mem.getSize(), 0);
	if (ret != cudaSuccess) {
		logger->error("Cannot register memory block {} addr={:p} size={:#x} to CUDA: ret={}",
		              mem.getAddrSpaceId(), localBase, mem.getSize(), ret);
		return false;
	}

	void* devicePointer = nullptr;
	ret = cudaHostGetDevicePointer(&devicePointer, localBase, 0);
	if (ret != cudaSuccess) {
		logger->error("Cannot retrieve device pointer for IO memory: ret={}", ret);
		return false;
	}

	mm.createMapping(reinterpret_cast<uintptr_t>(devicePointer), 0, mem.getSize(),
	                 "CudaHostMem", masterPciEAddrSpaceId, mem.getAddrSpaceId());

	return true;
}

bool Gpu::makeAccessibleToPCIeAndVA(const MemoryBlock& mem)
{
	if (pImpl->gdr == nullptr) {
		logger->warn("GDRcopy not available");
		return false;
	}

	auto& mm = MemoryManager::get();

	try {
		auto path = mm.findPath(masterPciEAddrSpaceId, mem.getAddrSpaceId());
		// if first hop is the PCIe bus, we know that memory is off-GPU
		if (path.front() == mm.getPciAddressSpace()) {
			throw std::out_of_range("Memory block is outside of this GPU");
		}

	} catch (const std::out_of_range&) {
		logger->error("Trying to map non-GPU memory block");
		return false;
	}

	logger->debug("retrieve complete device pointer from point of view of GPU");
	// retrieve complete device pointer from point of view of GPU
	auto translation = mm.getTranslation(masterPciEAddrSpaceId,
	                                     mem.getAddrSpaceId());
	CUdeviceptr devptr = translation.getLocalAddr(0);

	int ret;

	// required to set this flag before mapping
	unsigned int enable = 1;
	ret = cuPointerSetAttribute(&enable, CU_POINTER_ATTRIBUTE_SYNC_MEMOPS, devptr);
	if (ret != CUDA_SUCCESS) {
		logger->error("Cannot set pointer attributes on memory block {}: {}",
		              mem.getAddrSpaceId(), ret);
		return false;
	}

	gdr_mh_t mh;
	ret = gdr_pin_buffer(pImpl->gdr, devptr, mem.getSize(), 0, 0, &mh);
	if (ret != 0) {
		logger->error("Cannot pin memory block {} via gdrcopy: {}",
		              mem.getAddrSpaceId(), ret);
		return false;
	}

	void* bar = nullptr;
	ret = gdr_map(pImpl->gdr, mh, &bar, mem.getSize());
	if (ret != 0) {
		logger->error("Cannot map memory block {} via gdrcopy: {}",
		              mem.getAddrSpaceId(), ret);
		return false;
	}

	gdr_info_t info;
	ret = gdr_get_info(pImpl->gdr, mh, &info);
	if (ret != 0) {
		logger->error("Cannot get info for mapping of memory block {}: {}",
		              mem.getAddrSpaceId(), ret);
		return false;
	}

	const uintptr_t offset = info.va - devptr;
	const uintptr_t userPtr = reinterpret_cast<uintptr_t>(bar) + offset;

	logger->debug("BAR ptr:          {:p}",  bar);
	logger->debug("info.va:          {:#x}", info.va);
	logger->debug("info.mapped_size: {:#x}", info.mapped_size);
	logger->debug("info.page_size:   {:#x}", info.page_size);
	logger->debug("offset:           {:#x}", offset);
	logger->debug("user pointer:     {:#x}", userPtr);

	// mapping to acceses memory block from process
	mm.createMapping(userPtr, 0, info.mapped_size, "GDRcopy",
	                 mm.getProcessAddressSpace(), mem.getAddrSpaceId());

	// retrieve bus address
	uint64_t addr[8];
	ret = gdr_map_dma(pImpl->gdr, mh, 3, 0, 0, addr, 8);

	for (int i = 0; i < ret; i++) {
		logger->debug("DMA addr[{}]:      {:#x}", i, addr[i]);
	}

	if (ret != 1) {
		logger->error("Only one DMA address per block supported at the moment");
		return false;
	}

	// mapping to access memory block from peer devices via PCIe
	mm.createMapping(addr[0], 0, mem.getSize(), "GDRcopyDMA",
	                 mm.getPciAddressSpace(), mem.getAddrSpaceId());

	return true;
}

bool
Gpu::makeAccessibleFromPCIeOrHostRam(const MemoryBlock& mem)
{
	// Check which kind of memory this is and where it resides
	// There are two possibilities:
	//  - Host memory not managed by CUDA
	//  - IO memory somewhere on the PCIe bus

	auto& mm = MemoryManager::get();

	bool isIoMemory = false;
	try {
		auto path = mm.findPath(mm.getPciAddressSpace(), mem.getAddrSpaceId());
		isIoMemory = true;
	} catch(const std::out_of_range&) {
		// not reachable via PCI -> not IO memory
	}

	if (isIoMemory) {
		logger->debug("Memory block {} is assumed to be IO memory",
		              mem.getAddrSpaceId());

		return registerIoMemory(mem);
	} else {
		logger->debug("Memory block {} is assumed to be non-CUDA host memory",
		              mem.getAddrSpaceId());

		return registerHostMemory(mem);
	}
}

void Gpu::memcpySync(const MemoryBlock& src, const MemoryBlock& dst, size_t size)
{
	auto& mm = MemoryManager::get();

	auto src_translation = mm.getTranslation(masterPciEAddrSpaceId,
	                                         src.getAddrSpaceId());
	const void* src_buf = reinterpret_cast<void*>(src_translation.getLocalAddr(0));

	auto dst_translation = mm.getTranslation(masterPciEAddrSpaceId,
	                                         dst.getAddrSpaceId());
	void* dst_buf = reinterpret_cast<void*>(dst_translation.getLocalAddr(0));

	cudaSetDevice(gpuId);
	cudaMemcpy(dst_buf, src_buf, size, cudaMemcpyDefault);
}

void Gpu::memcpyKernel(const MemoryBlock& src, const MemoryBlock& dst, size_t size)
{
	auto& mm = MemoryManager::get();

	auto src_translation = mm.getTranslation(masterPciEAddrSpaceId,
	                                         src.getAddrSpaceId());
	auto src_buf = reinterpret_cast<uint8_t*>(src_translation.getLocalAddr(0));

	auto dst_translation = mm.getTranslation(masterPciEAddrSpaceId,
	                                         dst.getAddrSpaceId());
	auto dst_buf = reinterpret_cast<uint8_t*>(dst_translation.getLocalAddr(0));

	cudaSetDevice(gpuId);
	kernel_memcpy<<<1, 1>>>(dst_buf, src_buf, size);
	cudaDeviceSynchronize();
}

MemoryTranslation
Gpu::translate(const MemoryBlock& dst)
{
	auto& mm = MemoryManager::get();
	return mm.getTranslation(masterPciEAddrSpaceId, dst.getAddrSpaceId());
}


std::unique_ptr<villas::MemoryBlock, villas::MemoryBlock::deallocator_fn>
GpuAllocator::allocateBlock(size_t size)
{
	cudaSetDevice(gpu.gpuId);

	void* addr;
	auto& mm = MemoryManager::get();

	// search for an existing chunk that has enough free memory
	auto chunk = std::find_if(chunks.begin(), chunks.end(), [&](const auto& chunk) {
		return chunk->getAvailableMemory() >= size;
	});


	if (chunk != chunks.end()) {
		logger->debug("Found existing chunk that can host the requested block");

		return (*chunk)->allocateBlock(size);

	} else {
		// allocate a new chunk

		// rounded-up multiple of GPU page size
		const size_t chunkSize = size - (size & (GpuPageSize - 1)) + GpuPageSize;
		logger->debug("Allocate new chunk of {:#x} bytes", chunkSize);

		if (cudaSuccess != cudaMalloc(&addr, chunkSize)) {
			logger->error("cudaMalloc(..., size={}) failed", chunkSize);
			throw std::bad_alloc();
		}

		// assemble name for this block
		std::stringstream name;
		name << std::showbase << std::hex << reinterpret_cast<uintptr_t>(addr);

		auto blockName = mm.getSlaveAddrSpaceName(getName(), name.str());
		auto blockAddrSpaceId = mm.getOrCreateAddressSpace(blockName);

		const auto localAddr = reinterpret_cast<uintptr_t>(addr);
		std::unique_ptr<MemoryBlock, MemoryBlock::deallocator_fn>
		        mem(new MemoryBlock(localAddr, chunkSize, blockAddrSpaceId), this->free);

		insertMemoryBlock(*mem);

		// already make accessible to CPU
		gpu.makeAccessibleToPCIeAndVA(*mem);

		// create a new allocator to manage the chunk and push to chunk list
		chunks.push_front(std::make_unique<LinearAllocator>(std::move(mem)));

		// call again, this time there's a large enough chunk
		return allocateBlock(size);
	}
}


Gpu::Gpu(int gpuId) :
    pImpl{std::make_unique<impl>()},
    gpuId(gpuId)
{
	logger = villas::logging.get(getName());

	pImpl->gdr = gdr_open();
	if (pImpl->gdr == nullptr) {
		logger->warn("No GDRcopy support enabled, cannot open /dev/gdrdrv");
	}
}

bool Gpu::init()
{
	auto& mm = MemoryManager::get();

	const auto gpuPciEAddrSpaceName = mm.getMasterAddrSpaceName(getName(), "pcie");
	masterPciEAddrSpaceId = mm.getOrCreateAddressSpace(gpuPciEAddrSpaceName);

	allocator = std::make_unique<GpuAllocator>(*this);

	cudaDeviceProp deviceProp;
	cudaGetDeviceProperties(&deviceProp, gpuId);

	pImpl->pdev.slot = {
	    deviceProp.pciDomainID,
	    deviceProp.pciBusID,
	    deviceProp.pciDeviceID,
	    0};

	struct pci_region* pci_regions = nullptr;
	const size_t pci_num_regions = pci_get_regions(&pImpl->pdev, &pci_regions);
	for (size_t i = 0; i < pci_num_regions; i++) {
		const size_t region_size = pci_regions[i].end - pci_regions[i].start + 1;
		logger->info("BAR{}: bus addr={:#x} size={:#x}",
		             pci_regions[i].num, pci_regions[i].start, region_size);

		char name[] = "BARx";
		name[3] = '0' + pci_regions[i].num;

		auto gpuBarXAddrSpaceName = mm.getSlaveAddrSpaceName(getName(), name);
		auto gpuBarXAddrSpaceId = mm.getOrCreateAddressSpace(gpuBarXAddrSpaceName);

		mm.createMapping(pci_regions[i].start, 0, region_size,
		                 std::string("PCI-") + name,
		                 mm.getPciAddressSpace(), gpuBarXAddrSpaceId);
	}

	free(pci_regions);

	return true;
}


std::list<std::unique_ptr<Gpu>>
GpuFactory::make()
{
	int deviceCount = 0;
	cudaGetDeviceCount(&deviceCount);

	std::list<std::unique_ptr<Gpu>> gpuList;

	for (int gpuId = 0; gpuId < deviceCount; gpuId++) {
		if (cudaSetDevice(gpuId) != cudaSuccess) {
			logger->warn("Cannot activate GPU {}", gpuId);
			continue;
		}

		auto gpu = std::make_unique<Gpu>(gpuId);

		if (not gpu->init()) {
			logger->warn("Cannot initialize GPU {}", gpuId);
			continue;
		}

		gpuList.emplace_back(std::move(gpu));
	}

	logger->info("Initialized {} GPUs", gpuList.size());
	for (auto& gpu : gpuList) {
		logger->debug(" - {}", gpu->getName());
	}

	return gpuList;
}

} // namespace villas
} // namespace gpu
