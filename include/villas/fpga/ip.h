#ifndef _IP_H_
#define _IP_H_

#include <stdint.h>

#include <xilinx/xtmrctr.h>
#include <xilinx/xintc.h>
#include <xilinx/xllfifo.h>
#include <xilinx/xaxis_switch.h>
#include <xilinx/xaxidma.h>

#include "fpga/dma.h"
#include "fpga/switch.h"
#include "fpga/fifo.h"
#include "fpga/rtds_axis.h"
#include "fpga/model.h"
#include "nodes/fpga.h"

enum ip_state {
	IP_STATE_UNKNOWN,
	IP_STATE_INITIALIZED
};

struct ip {
	char *name;
	
	struct {
		char *vendor;
		char *library;
		char *name;
		char *version;
	} vlnv;

	uintptr_t baseaddr;
	uintptr_t baseaddr_axi4;
	int irq;
	int port;

	enum ip_state state;

	union {
		struct model model;
		struct bram {
			int size;
		} bram;

		XTmrCtr timer;
		XLlFifo fifo_mm;
		XAxiDma dma;
		XAxis_Switch sw;
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

int ip_parse(struct ip *c, config_setting_t *cfg);

#endif