/* FPGA related code for bootstrapping the unit-tests
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2018 Steffen Vogel <post@steffenvogel.de>
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>

#include <villas/fpga/core.hpp>
#include <villas/fpga/pcie_card.hpp>
#include <villas/fpga/utils.hpp>
#include <villas/fpga/vlnv.hpp>

#include "global.hpp"

#include <spdlog/spdlog.h>

#define FPGA_CARD "vc707"
#define TEST_CONFIG "../etc/fpga.json"
#define TEST_LEN 0x1000

#define CPU_HZ 3392389000
#define FPGA_AXI_HZ 125000000

using namespace villas;

static kernel::devices::PciDeviceList *pciDevices;

FpgaState state;

static void init() {
  FILE *f;
  json_error_t err;

  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%T] [%l] [%n] %v");

  plugin::registry->dump();

  pciDevices = kernel::devices::PciDeviceList::getInstance();

  auto vfioContainer = std::make_shared<kernel::vfio::Container>();

  // Parse FPGA configuration
  char *fn = getenv("TEST_CONFIG");
  f = fopen(fn ? fn : TEST_CONFIG, "r");
  cr_assert_not_null(f, "Cannot open config file");

  json_t *json = json_loadf(f, 0, &err);
  cr_assert_not_null(json, "Cannot load JSON config");

  fclose(f);

  json_t *fpgas = json_object_get(json, "fpgas");
  cr_assert_not_null(fpgas, "No section 'fpgas' found in config");
  cr_assert(json_object_size(json) > 0, "No FPGAs defined in config");

  // Get the FPGA card plugin
  auto fpgaCardFactory =
      plugin::registry->lookup<fpga::PCIeCardFactory>("pcie");
  cr_assert_not_null(fpgaCardFactory, "No plugin for FPGA card found");

  // Create all FPGA card instances using the corresponding plugin
  auto configDir = std::filesystem::path(fn).parent_path();
  auto cards = std::list<std::shared_ptr<fpga::Card>>();
  fpga::createCards(json, cards, configDir);
  state.cards = cards;

  cr_assert(state.cards.size() != 0, "No FPGA cards found!");

  json_decref(json);
}

static void fini() {
  // Release all cards
  state.cards.clear();
}

// cppcheck-suppress unknownMacro
TestSuite(fpga, .init = init, .fini = fini, .description = "VILLASfpga");
