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

/* Forward declarations */
class NodeCompat;

using namespace villas;

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

	struct {
		struct {
			MemoryAccessor<int32_t> i;
			MemoryAccessor<float> f;
		} accessor;
		MemoryBlock::Ptr block;
	} in, out;

	// Config only
	std::string cardName;
	std::string intfName;
	std::string dmaName;

public:
	FpgaNode();
	virtual ~FpgaNode();

	static int
	typeStart(node::SuperNode *sn);

	static int
	typeStop();

	virtual int
	parse(json_t *cfg);

	virtual char *
	print();

	virtual int
	check();

	virtual int
	prepare();

	virtual int
	read(struct sample *smps[], unsigned cnt, unsigned *release);

	virtual int
	write(struct sample *smps[], unsigned cnt, unsigned *release);

	virtual int
	pollFDs(int fds[]);
};

} /* namespace node */
} /* namespace villas */
