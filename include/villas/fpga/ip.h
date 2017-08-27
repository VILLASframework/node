/** Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <stdint.h>

#include "common.h"

#include "fpga/vlnv.h"

#include "fpga/ips/dma.h"
#include "fpga/ips/switch.h"
#include "fpga/ips/fifo.h"
#include "fpga/ips/rtds_axis.h"
#include "fpga/ips/timer.h"
#include "fpga/ips/model.h"
#include "fpga/ips/dft.h"
#include "fpga/ips/intc.h"

enum fpga_ip_types {
	FPGA_IP_TYPE_DM_DMA,	/**< A datamover IP exchanges streaming data between the FPGA and the CPU. */
	FPGA_IP_TYPE_DM_FIFO,	/**< A datamover IP exchanges streaming data between the FPGA and the CPU. */
	FPGA_IP_TYPE_MODEL,	/**< A model IP simulates a system on the FPGA. */
	FPGA_IP_TYPE_MATH,	/**< A math IP performs some kind of mathematical operation on the streaming data */
	FPGA_IP_TYPE_MISC,	/**< Other IP components like timer, counters, interrupt conctrollers or routing. */
	FPGA_IP_TYPE_INTERFACE	/**< A interface IP connects the FPGA to another system or controller. */
} type;

struct fpga_ip_type {
	struct fpga_vlnv vlnv;

	enum fpga_ip_types type;

	int (*init)(struct fpga_ip *c);
	int (*parse)(struct fpga_ip *c, json_t *cfg);
	int (*check)(struct fpga_ip *c);
	int (*start)(struct fpga_ip *c);
	int (*stop)(struct fpga_ip *c);
	int (*destroy)(struct fpga_ip *c);
	int (*reset)(struct fpga_ip *c);
	void (*dump)(struct fpga_ip *c);

	size_t size;			/**< Amount of memory which should be reserved for struct fpga_ip::_vd */
};

struct fpga_ip {
	char *name;			/**< Name of the FPGA IP component. */
	struct fpga_vlnv vlnv;		/**< The Vendor, Library, Name, Version tag of the FPGA IP component. */

	enum state state;		/**< The current state of the FPGA IP component. */

	struct fpga_ip_type *_vt;	/**< Vtable containing FPGA IP type function pointers. */
	void *_vd;			/**< Virtual data (used by struct fpga_ip::_vt functions) */

	uintptr_t baseaddr;		/**< The baseadress of this FPGA IP component */
	uintptr_t baseaddr_axi4;	/**< Used by AXI4 FIFO DM */

	int port;			/**< The port of the AXI4-Stream switch to which this FPGA IP component is connected. */
	int irq;			/**< The interrupt number of the FPGA IP component. */

	struct fpga_card *card;		/**< The FPGA to which this IP instance belongs to. */
};

/** Initialize IP core. */
int fpga_ip_init(struct fpga_ip *c, struct fpga_ip_type *vt);

/** Parse IP core configuration from configuration file */
int fpga_ip_parse(struct fpga_ip *c, json_t *cfg, const char *name);

/** Check configuration of IP core. */
int fpga_ip_check(struct fpga_ip *c);

/** Start IP core. */
int fpga_ip_start(struct fpga_ip *c);

/** Stop IP core. */
int fpga_ip_stop(struct fpga_ip *c);

/** Release dynamic memory allocated by this IP core. */
int fpga_ip_destroy(struct fpga_ip *c);

/** Dump details about this IP core to stdout. */
void fpga_ip_dump(struct fpga_ip *c);

/** Reset IP component to its initial state. */
int fpga_ip_reset(struct fpga_ip *c);

/** Find a registered FPGA IP core type with the given VLNV identifier. */
struct fpga_ip_type * fpga_ip_type_lookup(const char *vstr);


/** @} */
