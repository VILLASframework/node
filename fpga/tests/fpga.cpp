/** FPGA related code for bootstrapping the unit-tests
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Steffen Vogel
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

#include <villas/utils.h>
#include <villas/fpga/ip.h>
#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>

#include "global.hpp"

#include <spdlog/spdlog.h>

#define FPGA_CARD	"vc707"
#define TEST_CONFIG	"../etc/fpga.json"
#define TEST_LEN	0x1000

#define CPU_HZ		3392389000
#define FPGA_AXI_HZ	125000000

static struct pci pci;
static struct vfio_container vc;

FpgaState state;

static void init()
{
	int ret;

	FILE *f;
	json_error_t err;

	villas::Plugin::dumpList();

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
	state.cards = fpgaCardPlugin->make(fpgas, &pci, &vc);

	cr_assert(state.cards.size() != 0, "No FPGA cards found!");

	json_decref(json);
}

static void fini()
{
	// release all cards
	state.cards.clear();
}

TestSuite(fpga,
	.init = init,
	.fini = fini,
	.description = "VILLASfpga"
);
