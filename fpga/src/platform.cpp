/* Entrypoint for platform fpga tests
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include "villas/fpga/ips/switch.hpp"
#include <spdlog/common.h>
#include <villas/fpga/card/platform_card.hpp>
#include <villas/fpga/card/platform_card_factory.hpp>
#include <villas/fpga/tests.hpp>
#include <villas/fpga/utils.hpp>

int main() {
  logging.setLevel(spdlog::level::trace);

  std::string configFilePath = "../../../etc/fpga/miob/fpgas.json";
  auto configDir = std::filesystem::path(configFilePath).parent_path();
  std::vector<std::string> modules{"vfio"};
  auto vfioContainer = std::make_shared<kernel::vfio::Container>(modules);

  // Parse FPGA configuration
  FILE *f = fopen(configFilePath.c_str(), "r");
  if (!f)
    throw RuntimeError("Cannot open config file: {}", configFilePath);

  json_t *json = json_loadf(f, 0, nullptr);
  if (!json) {
    fclose(f);
    throw RuntimeError("Cannot parse JSON config");
  }

  fclose(f);

  json_t *fpgas = json_object_get(json, "fpgas");
  if (fpgas == nullptr) {
    exit(1);
  }

  json_t *fpga = json_object_get(fpgas, "zcu106");
  if (fpga == nullptr) {
    exit(1);
  }

  // Create all FPGA card instances using the corresponding plugin
  auto card =
      PlatformCardFactory::make(fpga, "zcu106", vfioContainer, configDir);

  auto axi_switch = std::dynamic_pointer_cast<fpga::ip::AxiStreamSwitch>(
      card->lookupIp(fpga::Vlnv("xilinx.com:ip:axis_switch:")));

  // Configure Axi-Switch for DMA loopback
  axi_switch->connectInternal("S00_AXIS", "M00_AXIS");

  Tests::writeRead(card);

  return 0;
}