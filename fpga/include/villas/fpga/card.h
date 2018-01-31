/** FPGA card
 *
 * This class represents a FPGA device.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <jansson.h>

#include "common.h"
#include "kernel/pci.h"
#include "kernel/vfio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct fpga_ip;
struct vfio_container;

struct fpga_card {
	char *name;			/**< The name of the FPGA card */

	enum state state;		/**< The state of this FPGA card. */

	struct pci *pci;
	struct pci_device filter;		/**< Filter for PCI device. */

	struct vfio_container *vfio_container;
	struct vfio_device vfio_device;	/**< VFIO device handle. */

	int do_reset;			/**< Reset VILLASfpga during startup? */
	int affinity;			/**< Affinity for MSI interrupts */

	struct list ips;		/**< List of IP components on FPGA. */

	char *map;			/**< PCI BAR0 mapping for register access */

	size_t maplen;
	size_t dmalen;

	/* Some IP cores are special and referenced here */
	struct fpga_ip *intc;
	struct fpga_ip *reset;
	struct fpga_ip *sw;
};

/** Initialize FPGA card and its IP components. */
int fpga_card_init(struct fpga_card *c, struct pci *pci, struct vfio_container *vc);

/** Parse configuration of FPGA card including IP cores from config. */
int fpga_card_parse(struct fpga_card *c, json_t *cfg, const char *name);

int fpga_card_parse_list(struct list *l, json_t *cfg);

/** Check if the FPGA card configuration is plausible. */
int fpga_card_check(struct fpga_card *c);

/** Start FPGA card. */
int fpga_card_start(struct fpga_card *c);

/** Stop FPGA card. */
int fpga_card_stop(struct fpga_card *c);

/** Destroy FPGA card. */
int fpga_card_destroy(struct fpga_card *c);

/** Dump details of FPGA card to stdout. */
void fpga_card_dump(struct fpga_card *c);

/** Reset the FPGA to a known state */
int fpga_card_reset(struct fpga_card *c);

#ifdef __cplusplus
}
#endif

/** @} */
