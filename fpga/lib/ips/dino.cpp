/** Driver for wrapper around standard Xilinx Aurora (xilinx.com:ip:aurora_8b10b)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017-2022, Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <cstdint>

#include <villas/utils.hpp>

#include <villas/fpga/ips/dino.hpp>

using namespace villas::fpga::ip;

static char n[] = "dino";
static char d[] = "Analog and Digital IO";
static char v[] = "xilinx.com:module_ref:dino:";
static NodePlugin<Dino, n, d, v> f;
