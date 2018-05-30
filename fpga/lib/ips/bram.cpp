#include "fpga/ips/bram.hpp"

namespace villas {
namespace fpga {
namespace ip {

static BramFactory factory;

bool
BramFactory::configureJson(IpCore& ip, json_t* json_ip)
{
	auto& bram = dynamic_cast<Bram&>(ip);

	if(json_unpack(json_ip, "{ s: i }", "size", &bram.size) != 0) {
		getLogger()->error("Cannot parse 'size'");
		return false;
	}

	return true;
}

bool Bram::init()
{
	allocator = std::make_unique<LinearAllocator>
	            (getAddressSpaceId(memoryBlock), this->size, 0);

	return true;
}

} // namespace ip
} // namespace fpga
} // namespace villas
