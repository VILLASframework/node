#ifndef _FPGA_IP_H_
#define _FPGA_IP_H_

#include <stdint.h>

#include <xilinx/xtmrctr.h>
#include <xilinx/xintc.h>
#include <xilinx/xllfifo.h>
#include <xilinx/xaxis_switch.h>
#include <xilinx/xaxidma.h>

#include "utils.h"
#include "fpga/dma.h"
#include "fpga/switch.h"
#include "fpga/fifo.h"
#include "fpga/rtds_axis.h"
#include "fpga/timer.h"
#include "fpga/model.h"
#include "fpga/dft.h"
#include "fpga/intc.h"
#include "nodes/fpga.h"

extern struct list ip_types;	/**< Table of existing FPGA IP core drivers */

enum ip_state {
	IP_STATE_UNKNOWN,
	IP_STATE_INITIALIZED
};

struct ip_vlnv {
	char *vendor;
	char *library;
	char *name;
	char *version;
};

struct ip_type {
	struct ip_vlnv vlnv;

	int (*parse)(struct ip *c);
	int (*init)(struct ip *c);
	int (*reset)(struct ip *c);
	void (*dump)(struct ip *c);
	void (*destroy)(struct ip *c);
};

struct ip {
	char *name;

	struct ip_vlnv vlnv;

	uintptr_t baseaddr;
	uintptr_t baseaddr_axi4;

	int port, irq;

	enum ip_state state;

	struct ip_type *_vt;

	union {
		struct model model;
		struct timer timer;
		struct fifo fifo;
		struct dma dma;
		struct sw sw;
		struct dft dft;
		struct intc intc;
	};
	
	struct fpga *card;

	config_setting_t *cfg;
};

/** Return the first IP block in list \p l which matches the VLNV */
struct ip * ip_vlnv_lookup(struct list *l, const char *vendor, const char *library, const char *name, const char *version);

/** Check if IP block \p c matched VLNV. */
int ip_vlnv_match(struct ip *c, const char *vendor, const char *library, const char *name, const char *version);

/** Tokenizes VLNV \p vlnv and stores it into \p c */
int ip_vlnv_parse(struct ip *c, const char *vlnv);

int ip_init(struct ip *c);

void ip_destroy(struct ip *c);

void ip_dump(struct ip *c);

int ip_reset(struct ip *c);

int ip_parse(struct ip *c, config_setting_t *cfg);

#endif /* _FPGA_IP_H_ */