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
 * @addtogroup villas_fpga BSD villas_fpga Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/io.h>
#include <villas/timing.h>

struct villas_fpga {
	std::string fpga_name;

	std::shared_ptr<villas::fpga::PCIeCard> card;

	fpga::IpNode *dma;
	fpga::IpNode *intf;

	fpga::Mem mem;
	fpga::Block block;
};

/** @see node_vtable::type_start */
int villas_fpga_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int villas_fpga_type_stop();

/** @see node_type::init */
int villas_fpga_init(struct node *n);

/** @see node_type::destroy */
int villas_fpga_destroy(struct node *n);

/** @see node_type::parse */
int villas_fpga_parse(struct node *n, json_t *cfg);

/** @see node_type::print */
char * villas_fpga_print(struct node *n);

/** @see node_type::check */
int villas_fpga_check();

/** @see node_type::prepare */
int villas_fpga_prepare();

/** @see node_type::start */
int villas_fpga_start(struct node *n);

/** @see node_type::stop */
int villas_fpga_stop(struct node *n);

/** @see node_type::pause */
int villas_fpga_pause(struct node *n);

/** @see node_type::resume */
int villas_fpga_resume(struct node *n);

/** @see node_type::write */
int villas_fpga_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::read */
int villas_fpga_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::reverse */
int villas_fpga_reverse(struct node *n);

/** @see node_type::poll_fds */
int villas_fpga_poll_fds(struct node *n, int fds[]);

/** @see node_type::netem_fds */
int villas_fpga_netem_fds(struct node *n, int fds[]);

/** @} */
