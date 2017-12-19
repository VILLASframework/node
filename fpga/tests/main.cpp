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

#define FPGA_CARD	"vc707"
#define TEST_CONFIG	"../etc/fpga.json"
#define TEST_LEN	0x1000

#define CPU_HZ		3392389000
#define FPGA_AXI_HZ	125000000

struct list cards;
struct fpga_card *card;
struct pci pci;
struct vfio_container vc;

static void init()
{
	int ret;

	FILE *f;
	json_error_t err;

	villas::Plugin::dumpList();

	Logger::setLogLevel(Logger::LogLevel::Debug);

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

	// create an FPGA card instance using the corresponding plugin
//	villas::FpgaCard* fpgaCard = fpgaCardPlugin->make(json_);

	auto fpgaCards = fpgaCardPlugin->make(fpgas, &pci, &vc);

	json_t *json_card = json_object_get(fpgas, FPGA_CARD);
	cr_assert_not_null(json_card, "FPGA card " FPGA_CARD " not found");

	card = (struct fpga_card *) alloc(sizeof(struct fpga_card));
	cr_assert_not_null(card, "Cannot allocate memory for FPGA card");

	ret = fpga_card_init(card, &pci, &vc);
	cr_assert_eq(ret, 0, "FPGA card initialization failed");

	ret = fpga_card_start(card);
	cr_assert_eq(ret, 0, "FPGA card cannot be started");

	ret = fpga_card_parse(card, json_card, FPGA_CARD);
	cr_assert_eq(ret, 0, "Failed to parse FPGA config");

	ret = fpga_card_check(card);
	cr_assert_eq(ret, 0, "FPGA card check failed");

	json_decref(json);

	if (criterion_options.logging_threshold < CRITERION_IMPORTANT)
		fpga_card_dump(card);
}

static void fini()
{
	int ret;

	ret = fpga_card_destroy(card);
	cr_assert_eq(ret, 0, "Failed to de-initilize FPGA");
}

TestSuite(fpga,
	.init = init,
	.fini = fini,
	.description = "VILLASfpga"
);
