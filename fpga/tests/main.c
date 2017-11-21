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

#define TEST_CONFIG	"/villas/etc/fpga.conf"
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
	json_t *json;

	ret = pci_init(&pci);
	cr_assert_eq(ret, 0, "Failed to initialize PCI sub-system");

	ret = vfio_init(&vc);
	cr_assert_eq(ret, 0, "Failed to initiliaze VFIO sub-system");

	/* Parse FPGA configuration */
	f = fopen(TEST_CONFIG, "r");
	cr_assert_not_null(f);

	json = json_loadf(f, 0, &err);
	cr_assert_not_null(json);

	fclose(f);

	list_init(&cards);
	ret = fpga_card_parse_list(&cards, json);
	cr_assert_eq(ret, 0, "Failed to parse FPGA config");

	json_decref(json);

	card = list_lookup(&cards, "vc707");
	cr_assert(card, "FPGA card not found");

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
