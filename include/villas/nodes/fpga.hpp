/** Communicate with VILLASfpga Xilinx FPGA boards
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/format.hpp>
#include <villas/timing.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/node.hpp>
#include <villas/fpga/ips/dma.hpp>

namespace villas {
namespace node {

#define FPGA_DMA_VLNV
#define FPGA_AURORA_VLNV "acs.eonerc.rwth-aachen.de:user:aurora_axis:"

class FpgaNode : public Node {

protected:
	int irqFd;
	int coalesce;
	bool polling;

	std::shared_ptr<fpga::PCIeCard> card;

	std::shared_ptr<fpga::ip::Dma> dma;
	std::shared_ptr<fpga::ip::Node> intf;

	std::unique_ptr<const MemoryBlock> blockRx;
	std::unique_ptr<const MemoryBlock> blockTx;

	// Config only
	std::string cardName;
	std::string intfName;
	std::string dmaName;

protected:
	virtual int
	_read(Sample *smps[], unsigned cnt);

	virtual int
	_write(Sample *smps[], unsigned cnt);

public:
	FpgaNode(const std::string &name = "");
	virtual ~FpgaNode();

	virtual
	int parse(json_t *cfg, const uuid_t sn_uuid);

	virtual
	const std::string & getDetails();

	virtual
	int check();

	virtual
	int prepare();

	virtual
	int start();

	virtual
	int stop();

	virtual
	std::vector<int> getPollFDs();
};


class FpgaNodeFactory : public NodeFactory {

public:
	using NodeFactory::NodeFactory;

	virtual
	Node * make()
	{
		auto *n = new FpgaNode;

		init(n);

		return n;
	}

	virtual
	int getFlags() const
	{
		return (int) NodeFactory::Flags::SUPPORTS_READ |
		       (int) NodeFactory::Flags::SUPPORTS_WRITE |
		       (int) NodeFactory::Flags::SUPPORTS_POLL;
	}

	virtual
	std::string getName() const
	{
		return "fpga";
	}

	virtual
	std::string getDescription() const
	{
		return "VILLASfpga";
	}

	virtual
	int start(SuperNode *sn);

	virtual
	int stop();
};

} /* namespace node */
} /* namespace villas */
