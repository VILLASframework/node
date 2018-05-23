#include <unistd.h>
#include <cstring>

#include <villas/memory_manager.hpp>
#include <villas/fpga/ips/rtds2gpu.hpp>

#include "log.hpp"

namespace villas {
namespace fpga {
namespace ip {

static Rtds2GpuFactory factory;

bool Rtds2Gpu::init()
{
	xInstance.IsReady = XIL_COMPONENT_IS_READY;
	xInstance.Ctrl_BaseAddress = getBaseAddr(registerMemory);

	// make sure IP is stopped for now
	XRtds2gpu_DisableAutoRestart(&xInstance);

	dump();

	return true;
}

void Rtds2Gpu::dump()
{
	const auto baseaddr = XRtds2gpu_Get_baseaddr(&xInstance);
	const auto data_offset = XRtds2gpu_Get_data_offset(&xInstance);
	const auto doorbell_offset = XRtds2gpu_Get_doorbell_offset(&xInstance);
	const auto frame_size = XRtds2gpu_Get_frame_size(&xInstance);
	const auto status = XRtds2gpu_Get_status_o(&xInstance);

	logger->debug("Rtds2Gpu registers (IP base {:#x}):", xInstance.Ctrl_BaseAddress);
	logger->debug("  Base address (bytes):     {:#x}", baseaddr);
	logger->debug("  Doorbell offset (bytes):  {:#x}", doorbell_offset);
	logger->debug("  Data offset (bytes):      {:#x}", data_offset);
	logger->debug("  Frame size (words):       {:#x}", frame_size);
	logger->debug("  Status:                   {:#x}", status);
}

bool Rtds2Gpu::startOnce(const MemoryBlock& mem, size_t frameSize)
{
	auto& mm = MemoryManager::get();

	auto translationFromIp = mm.getTranslation(
	                               getMasterAddrSpaceByInterface(axiInterface),
	                               mem.getAddrSpaceId());

	// make sure IP is stopped for now
	XRtds2gpu_DisableAutoRestart(&xInstance);
//	while(not XRtds2gpu_IsIdle(&xInstance) and not XRtds2gpu_IsDone(&xInstance));

	// set address of memory block in HLS IP
	XRtds2gpu_Set_baseaddr(&xInstance, translationFromIp.getLocalAddr(0));

	XRtds2gpu_Set_doorbell_offset(&xInstance, 0);
	XRtds2gpu_Set_data_offset(&xInstance, 4);
	XRtds2gpu_Set_frame_size(&xInstance, frameSize);

	// prepare memory with all zeroes
	auto translationFromProcess = mm.getTranslationFromProcess(mem.getAddrSpaceId());
	auto memory = reinterpret_cast<void*>(translationFromProcess.getLocalAddr(0));
	memset(memory, 0, mem.getSize());

	// start IP
//	XRtds2gpu_EnableAutoRestart(&xInstance);
	XRtds2gpu_Start(&xInstance);

	return true;
}

bool Rtds2Gpu::isDone()
{
	return XRtds2gpu_IsDone(&xInstance);
}

Rtds2GpuFactory::Rtds2GpuFactory() :
    IpNodeFactory(getName())
{
}

} // namespace ip
} // namespace fpga
} // namespace villas
