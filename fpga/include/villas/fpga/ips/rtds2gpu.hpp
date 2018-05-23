#pragma once

#include <villas/memory.hpp>
#include <villas/fpga/ip_node.hpp>

#include "rtds2gpu/xrtds2gpu.h"

namespace villas {
namespace fpga {
namespace ip {


class Rtds2Gpu : public IpNode
{
public:
	friend class Rtds2GpuFactory;

	bool init();

	void dump();

	bool startOnce(const MemoryBlock& mem, size_t frameSize);

	bool isDone();

private:
	static constexpr const char* registerMemory = "Reg";
	static constexpr const char* axiInterface = "m_axi_axi_mm";
	static constexpr const char* streamInterface = "rtds_input";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{ return { registerMemory }; }

	XRtds2gpu xInstance;
};


class Rtds2GpuFactory : public IpNodeFactory {
public:
	Rtds2GpuFactory();

	IpCore* create()
	{ return new Rtds2Gpu; }

	std::string
	getName() const
	{ return "Rtds2Gpu"; }

	std::string
	getDescription() const
	{ return "HLS RTDS2GPU IP"; }

	Vlnv getCompatibleVlnv() const
	{ return {"xilinx.com:hls:rtds2gpu:"}; }
};

} // namespace ip
} // namespace fpga
} // namespace villas
