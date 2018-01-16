/** Main Unit Test entry point.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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

#include <criterion/criterion.h>
#include <criterion/options.h>

#include <villas/utils.h>
#include <villas/fpga/ip.h>
#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>

#include <villas/log.hpp>
#include <villas/plugin.hpp>
#include <villas/fpga/card.hpp>

#include <spdlog/spdlog.h>

#define FPGA_CARD	"vc707"
#define TEST_CONFIG	"../etc/fpga.json"
#define TEST_LEN	0x1000

#define CPU_HZ		3392389000
#define FPGA_AXI_HZ	125000000

struct pci pci;
struct vfio_container vc;
villas::fpga::CardList fpgaCards;
villas::fpga::PCIeCard* fpga;

// keep to make it compile with old C tests
struct fpga_card* card;

static void init()
{
	int ret;

	FILE *f;
	json_error_t err;

	villas::Plugin::dumpList();

	auto logger = loggerGetOrCreate("unittest");
	spdlog::set_pattern("[%T] [%l] [%n] %v");
	spdlog::set_level(spdlog::level::debug);

	ret = pci_init(&pci);
	cr_assert_eq(ret, 0, "Failed to initialize PCI sub-system");

	ret = vfio_init(&vc);
	cr_assert_eq(ret, 0, "Failed to initiliaze VFIO sub-system");

	/* Parse FPGA configuration */
	f = fopen(TEST_CONFIG, "r");
	cr_assert_not_null(f, "Cannot open config file");

	json_t *json = json_loadf(f, 0, &err);
	cr_assert_not_null(json, "Cannot load JSON config");

	fclose(f);

	json_t *fpgas = json_object_get(json, "fpgas");
	cr_assert_not_null(fpgas, "No section 'fpgas' found in config");
	cr_assert(json_object_size(json) > 0, "No FPGAs defined in config");

	// get the FPGA card plugin
	villas::Plugin* plugin = villas::Plugin::lookup(villas::Plugin::Type::FpgaCard, "");
	cr_assert_not_null(plugin, "No plugin for FPGA card found");
	villas::fpga::PCIeCardFactory* fpgaCardPlugin = dynamic_cast<villas::fpga::PCIeCardFactory*>(plugin);

	// create all FPGA card instances using the corresponding plugin
	fpgaCards = fpgaCardPlugin->make(fpgas, &pci, &vc);

	if(fpgaCards.size() == 0) {
		logger->error("No FPGA cards found!");
	} else {
		fpga = fpgaCards.front().get();
	}

	cr_assert_not_null(fpga, "No FPGA card available");

	json_decref(json);
}

static void fini()
{
	fpgaCards.clear();
}

TestSuite(fpga,
	.init = init,
	.fini = fini,
	.description = "VILLASfpga"
);
