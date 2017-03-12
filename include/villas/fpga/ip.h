/** Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <stdint.h>

#include "common.h"

#include "nodes/fpga.h"

#include "fpga/vlnv.h"

#include "fpga/ips/dma.h"
#include "fpga/ips/switch.h"
#include "fpga/ips/fifo.h"
#include "fpga/ips/rtds_axis.h"
#include "fpga/ips/timer.h"
#include "fpga/ips/model.h"
#include "fpga/ips/dft.h"
#include "fpga/ips/intc.h"

struct fpga_ip_type {
	struct fpga_vlnv vlnv;

	enum {
		FPGA_IP_TYPE_DATAMOVER,	/**< A datamover IP exchanges streaming data between the FPGA and the CPU. */
		FPGA_IP_TYPE_MODEL,	/**< A model IP simulates a system on the FPGA. */
		FPGA_IP_TYPE_MATH,	/**< A math IP performs some kind of mathematical operation on the streaming data */
		FPGA_IP_TYPE_MISC,	/**< Other IP components like timer, counters, interrupt conctrollers or routing. */
		FPGA_IP_TYPE_INTERFACE	/**< A interface IP connects the FPGA to another system or controller. */
	} type;

	int (*parse)(struct fpga_ip *c);
	int (*init)(struct fpga_ip *c);
	int (*destroy)(struct fpga_ip *c);
	int (*reset)(struct fpga_ip *c);
	void (*dump)(struct fpga_ip *c);
};

struct fpga_ip {
	char *name;			/**< Name of the FPGA IP component. */
	struct fpga_vlnv vlnv;		/**< The Vendor, Library, Name, Version tag of the FPGA IP component. */

	enum state state;		/**< The current state of the FPGA IP component. */

	struct fpga_ip_type *_vt;	/**< Vtable containing FPGA IP type function pointers. */

	uintptr_t baseaddr;		/**< The baseadress of this FPGA IP component */
	uintptr_t baseaddr_axi4;	/**< Used by AXI4 FIFO DM */

	int port;			/**< The port of the AXI4-Stream switch to which this FPGA IP component is connected. */
	int irq;			/**< The interrupt number of the FPGA IP component. */

	union {
		struct model model;
		struct timer timer;
		struct fifo fifo;
		struct dma dma;
		struct sw sw;
		struct dft dft;
		struct intc intc;
	};				/**< Specific private date per FPGA IP type. Depends on fpga_ip::_vt */
	
	struct fpga_card *card;		/**< The FPGA to which this IP instance belongs to. */

	config_setting_t *cfg;
};

/** Initialize IP instance. */
int fpga_ip_init(struct fpga_ip *c);

/** Release dynamic memory allocated by this IP instance. */
int fpga_ip_destroy(struct fpga_ip *c);

/** Dump details about this IP instance to stdout. */
void fpga_ip_dump(struct fpga_ip *c);

/** Reset IP component to its initial state. */
int fpga_ip_reset(struct fpga_ip *c);

/** Parse IP configuration from configuration file */
int fpga_ip_parse(struct fpga_ip *c, config_setting_t *cfg);

/** @} */