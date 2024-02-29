/* Block-Raam related helper functions
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2018 Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/memory.hpp>
#include <villas/fpga/core.hpp>

namespace villas {
namespace fpga {
namespace ip {

class BramFactory;

class Bram : public Core {
	friend class BramFactory;

public:

	virtual
	bool init() override;

	LinearAllocator& getAllocator()
	{
		return *allocator;
	}

private:
	static constexpr
	const char* memoryBlock = "Mem0";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{
		return {
			memoryBlock
		};
	}

	size_t size;
	std::unique_ptr<LinearAllocator> allocator;
};

class BramFactory : public CoreFactory {

public:
	virtual
	std::string getName() const
	{
		return "bram";
	}

	virtual
	std::string getDescription() const
	{
		return "Block RAM";
	}

private:
	virtual
	Vlnv getCompatibleVlnv() const
	{
		return Vlnv("xilinx.com:ip:axi_bram_ctrl:");
	}

	// Create a concrete IP instance
	Core* make() const
	{
		return new Bram;
	};

protected:
	virtual
	void parse(Core &, json_t *) override;
};

} // namespace ip
} // namespace fpga
} // namespace villas
