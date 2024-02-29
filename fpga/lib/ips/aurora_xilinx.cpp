/* Driver for wrapper around standard Xilinx Aurora (xilinx.com:ip:aurora_8b10b)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>

#include <villas/utils.hpp>

#include <villas/fpga/ips/aurora_xilinx.hpp>

using namespace villas::fpga::ip;

static char n[] = "aurora_xilinx";
static char d[] = "Xilinx Aurora 8B/10B.";
static char v[] = "xilinx.com:ip:aurora_8b10b:";
static NodePlugin<AuroraXilinx, n, d, v> f;
