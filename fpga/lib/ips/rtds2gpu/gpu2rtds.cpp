#include <unistd.h>
#include <cstring>

#include <villas/log.hpp>
#include <villas/memory_manager.hpp>
#include <villas/fpga/ips/gpu2rtds.hpp>

using namespace villas::fpga::ip;

static Gpu2RtdsFactory factory;

bool Gpu2Rtds::init()
{
	Hls::init();

	auto &registers = addressTranslations.at(registerMemory);

	registerStatus = reinterpret_cast<StatusRegister*>(registers.getLocalAddr(registerStatusOffset));
	registerStatusCtrl = reinterpret_cast<StatusControlRegister*>(registers.getLocalAddr(registerStatusCtrlOffset));
	registerFrameSize = reinterpret_cast<uint32_t*>(registers.getLocalAddr(registerFrameSizeOffset));
	registerFrames = reinterpret_cast<uint32_t*>(registers.getLocalAddr(registerFrameOffset));

	maxFrameSize = getMaxFrameSize();
	logger->info("Max. frame size supported: {}", maxFrameSize);

	return true;
}

bool
Gpu2Rtds::startOnce(size_t frameSize)
{
	*registerFrameSize = frameSize;

	start();

	return true;
}

void Gpu2Rtds::dump(spdlog::level::level_enum logLevel)
{
	const auto frame_size = *registerFrameSize;
	auto status = *registerStatus;

	logger->log(logLevel, "Gpu2Rtds registers:");
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

//bool Gpu2Rtds::startOnce(const MemoryBlock &mem, size_t frameSize, size_t dataOffset, size_t doorbellOffset)
//{
//	auto &mm = MemoryManager::get();

//	if (frameSize > maxFrameSize) {
//		logger->error("Requested frame size of {} exceeds max. frame size of {}",
//		              frameSize, maxFrameSize);
//		return false;
//	}

//	auto translationFromIp = mm.getTranslation(
//	                               getMasterAddrSpaceByInterface(axiInterface),
//	                               mem.getAddrSpaceId());

//	// set address of memory block in HLS IP
//	XGpu2Rtds_Set_baseaddr(&xInstance, translationFromIp.getLocalAddr(0));

//	XGpu2Rtds_Set_doorbell_offset(&xInstance, doorbellOffset);
//	XGpu2Rtds_Set_data_offset(&xInstance, dataOffset);
//	XGpu2Rtds_Set_frame_size(&xInstance, frameSize);

//	// prepare memory with all zeroes
//	auto translationFromProcess = mm.getTranslationFromProcess(mem.getAddrSpaceId());
//	auto memory = reinterpret_cast<void*>(translationFromProcess.getLocalAddr(0));
//	memset(memory, 0, mem.getSize());

//	// start IP
//	return start();
//}





//bool
//Gpu2Rtds::updateStatus()
//{
//	if (not XGpu2Rtds_Get_status_vld(&xInstance))
//		return false;

//	status.value = XGpu2Rtds_Get_status(&xInstance);

//	return true;
//}

size_t
Gpu2Rtds::getMaxFrameSize()
{
	*registerFrameSize = 0;

	start();
	while (not isFinished());

	while (not registerStatusCtrl->status_ap_vld);

	axilite_reg_status_t status = *registerStatus;

//	logger->debug("(*registerStatus).max_frame_size: {}", (*registerStatus).max_frame_size);
//	logger->debug("status.max_frame_size: {}", status.max_frame_size);

//	assert(status.max_frame_size == (*registerStatus).max_frame_size);

	return status.max_frame_size;
}

//void
//Gpu2Rtds::dumpDoorbell(uint32_t doorbellRegister) const
//{
//	auto &doorbell = reinterpret_cast<reg_doorbell_t&>(doorbellRegister);

//	logger->info("Doorbell register: {:#08x}", doorbell.value);
//	logger->info("  Valid:       {}", (doorbell.is_valid ? "yes" : "no"));
//	logger->info("  Count:       {}", doorbell.count);
//	logger->info("  Seq. number: {}", doorbell.seq_nr);
//}

