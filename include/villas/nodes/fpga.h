/** Node type: VILLASfpga
 *
 * This file implements the fpga node-type.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 *********************************************************************************/

/**
 * @addtogroup fpga VILLASfpga
 * @ingroup node
 * @{
 */

#pragma once

#include "kernel/vfio.h"
#include "kernel/pci.h"

#include "fpga/ips/dma.h"

#include "node.h"
#include "list.h"

/* Forward declarations */
struct fpga_ip;

/** The node type */
struct fpga {
	struct fpga_ip *ip;

	int use_irqs;

	struct dma_mem dma;

	enum {
		FPGA_DM_DMA,
		FPGA_DM_FIFO
	} type;
};

/** @see node_vtable::init */
int fpga_init(int argc, char *argv[], config_setting_t *cfg);

/** @see node_vtable::deinit */
int fpga_deinit();

/** @see node_vtable::parse */
int fpga_parse(struct node *n, config_setting_t *cfg);

/** Parse the 'fpga' section in the configuration file */
int fpga_parse_cards(config_setting_t *cfg);

struct fpga_card * fpga_lookup_card(const char *name);

/** @see node_vtable::print */
char * fpga_print(struct node *n);

/** @see node_vtable::open */
int fpga_start(struct node *n);

/** @see node_vtable::close */
int fpga_stop(struct node *n);

/** @see node_vtable::read */
int fpga_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_vtable::write */
int fpga_write(struct node *n, struct sample *smps[], unsigned cnt);

/** Get pointer to internal VILLASfpga datastructure */
struct fpga * fpga_get();

/** Reset VILLASfpga */
int fpga_reset(struct fpga *f);

/** Dump some details about the fpga card */
void fpga_dump(struct fpga *f);

/** @} */
