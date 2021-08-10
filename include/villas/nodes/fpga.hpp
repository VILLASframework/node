/** Communicate with VILLASfpga Xilinx FPGA boards
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <villas/node/config.hpp>
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

struct fpga_node {
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
};

int fpga_type_start(SuperNode *sn);

int fpga_type_stop();

int fpga_init(NodeCompat *n);

int fpga_destroy(NodeCompat *n);

int fpga_parse(NodeCompat *n, json_t *json);

char * fpga_print(NodeCompat *n);

int fpga_check(NodeCompat *n);

int fpga_prepare(NodeCompat *n);

int fpga_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int fpga_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int fpga_poll_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
