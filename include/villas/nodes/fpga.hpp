/** Communicate with VILLASfpga Xilinx FPGA boards
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @addtogroup fpga BSD fpga Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/format.hpp>
#include <villas/timing.h>

#include <villas/fpga/card.hpp>
#include <villas/fpga/node.hpp>
#include <villas/fpga/ips/dma.hpp>

using namespace villas;

#define FPGA_DMA_VLNV
#define FPGA_AURORA_VLNV "acs.eonerc.rwth-aachen.de:user:aurora_axis:"

struct fpga {
	int irqFd;
	int coalesce;
	bool polling;

	std::shared_ptr<villas::fpga::PCIeCard> card;

	std::shared_ptr<villas::fpga::ip::Dma> dma;
	std::shared_ptr<villas::fpga::ip::Node> intf;

	struct {
		villas::MemoryAccessor<uint32_t> mem;
		villas::MemoryBlock block;
	} in, out;

	// Config only
	std::string cardName;
	std::string intfName;
	std::string dmaName;
};

/** @see node_vtable::type_start */
int fpga_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int fpga_type_stop();

/** @see node_type::init */
int fpga_init(struct vnode *n);

/** @see node_type::destroy */
int fpga_destroy(struct vnode *n);

/** @see node_type::parse */
int fpga_parse(struct vnode *n, json_t *json);

/** @see node_type::print */
char * fpga_print(struct vnode *n);

/** @see node_type::check */
int fpga_check();

/** @see node_type::prepare */
int fpga_prepare();

/** @see node_type::start */
int fpga_start(struct vnode *n);

/** @see node_type::stop */
int fpga_stop(struct vnode *n);

/** @see node_type::write */
int fpga_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::read */
int fpga_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::poll_fds */
int fpga_poll_fds(struct vnode *n, int fds[]);

/** @} */
