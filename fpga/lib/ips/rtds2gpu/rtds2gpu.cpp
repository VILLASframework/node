#include <unistd.h>
#include <cstring>

#include <villas/log.hpp>
#include <villas/memory_manager.hpp>
#include <villas/fpga/ips/rtds2gpu.hpp>

namespace villas {
namespace fpga {
namespace ip {

static Rtds2GpuFactory factory;

bool Rtds2Gpu::init()
{
	Hls::init();

	xInstance.IsReady = XIL_COMPONENT_IS_READY;
	xInstance.Ctrl_BaseAddress = getBaseAddr(registerMemory);

	status.value = 0;
	started = false;

//	maxFrameSize = getMaxFrameSize();
	maxFrameSize = 16;
	logger->info("Max. frame size supported: {}", maxFrameSize);

	return true;
}



void Rtds2Gpu::dump(spdlog::level::level_enum logLevel)
{
	const auto baseaddr = XRtds2gpu_Get_baseaddr(&xInstance);
	const auto data_offset = XRtds2gpu_Get_data_offset(&xInstance);
	const auto doorbell_offset = XRtds2gpu_Get_doorbell_offset(&xInstance);
	const auto frame_size = XRtds2gpu_Get_frame_size(&xInstance);

	logger->log(logLevel, "Rtds2Gpu registers (IP base {:#x}):", xInstance.Ctrl_BaseAddress);
	logger->log(logLevel, "  Base address (bytes):     {:#x}", baseaddr);
	logger->log(logLevel, "  Doorbell offset (bytes):  {:#x}", doorbell_offset);
	logger->log(logLevel, "  Data offset (bytes):      {:#x}", data_offset);
	logger->log(logLevel, "  Frame size (words):       {:#x}", frame_size);
	logger->log(logLevel, "  Status:                   {:#x}", status.value);
	logger->log(logLevel, "    Running:            {}", (status.is_running ? "yes" : "no"));
	logger->log(logLevel, "    Frame too short:    {}", (status.frame_too_short ? "yes" : "no"));
	logger->log(logLevel, "    Frame too long:     {}", (status.frame_too_long ? "yes" : "no"));
	logger->log(logLevel, "    Frame size invalid: {}", (status.invalid_frame_size ? "yes" : "no"));
	logger->log(logLevel, "    Last count:         {}", status.last_count);
	logger->log(logLevel, "    Last seq. number:   {}", status.last_seq_nr);
	logger->log(logLevel, "    Max. frame size:    {}", status.max_frame_size);
}

bool Rtds2Gpu::startOnce(const MemoryBlock& mem, size_t frameSize, size_t dataOffset, size_t doorbellOffset)
{
	auto& mm = MemoryManager::get();

	if (frameSize > maxFrameSize) {
		logger->error("Requested frame size of {} exceeds max. frame size of {}",
		              frameSize, maxFrameSize);
		return false;
	}

	auto translationFromIp = mm.getTranslation(
	                               getMasterAddrSpaceByInterface(axiInterface),
	                               mem.getAddrSpaceId());

	// set address of memory block in HLS IP
	XRtds2gpu_Set_baseaddr(&xInstance, translationFromIp.getLocalAddr(0));

	XRtds2gpu_Set_doorbell_offset(&xInstance, doorbellOffset);
	XRtds2gpu_Set_data_offset(&xInstance, dataOffset);
	XRtds2gpu_Set_frame_size(&xInstance, frameSize);

	// prepare memory with all zeroes
	auto translationFromProcess = mm.getTranslationFromProcess(mem.getAddrSpaceId());
	auto memory = reinterpret_cast<void*>(translationFromProcess.getLocalAddr(0));
	memset(memory, 0, mem.getSize());

	// start IP
	return start();
}

bool
Rtds2Gpu::updateStatus()
{
	if (not XRtds2gpu_Get_status_vld(&xInstance))
		return false;

	status.value = XRtds2gpu_Get_status(&xInstance);

	return true;
}

size_t
Rtds2Gpu::getMaxFrameSize()
{
	XRtds2gpu_Set_frame_size(&xInstance, 0);

	start();
	while (not isFinished());
	updateStatus();

	return status.max_frame_size;
}

void
Rtds2Gpu::dumpDoorbell(uint32_t doorbellRegister) const
{
	auto& doorbell = reinterpret_cast<reg_doorbell_t&>(doorbellRegister);

	logger->info("Doorbell register: {:#08x}", doorbell.value);
	logger->info("  Valid:       {}", (doorbell.is_valid ? "yes" : "no"));
	logger->info("  Count:       {}", doorbell.count);
	logger->info("  Seq. number: {}", doorbell.seq_nr);
}

} // namespace ip
} // namespace fpga
} // namespace villas
