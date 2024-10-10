/* Zynq VFIO connector node
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>
#include <limits>

#include <villas/exceptions.hpp>
#include <villas/memory.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/zynq.hpp>
#include <villas/fpga/pcie_card.hpp>

#include <villas/fpga/platform_card.hpp>

using namespace villas::fpga::ip;

bool Zynq::init() {
  auto &mm = MemoryManager::get();

  // Save ID in card so we can create mappings later when needed (e.g. when
  // allocating DMA memory in host RAM)
  card->addrSpaceIdDeviceToHost =
      mm.getOrCreateAddressSpace("zynq_zynq_ultra_ps_e_0/HPC0_DDR_LOW");
  return true;
}

void ZynqFactory::parse(Core &ip, json_t *cfg) {
  CoreFactory::parse(ip, cfg);

  auto logger = getLogger();
}

static ZynqFactory p;
