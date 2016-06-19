#include <string.h>
#include <unistd.h>

#include <libconfig.h>

#include "log.h"

#include "fpga/ip.h"
#include "fpga/intc.h"
#include "fpga/fifo.h"
#include "nodes/fpga.h"

#include "config-fpga.h"

struct ip * ip_vlnv_lookup(struct list *l, const char *vendor, const char *library, const char *name, const char *version)
{
	list_foreach(struct ip *c, l) {
		if (ip_vlnv_match(c, vendor, library, name, version))
			return c;
	}

	return NULL;
}

int ip_vlnv_match(struct ip *c, const char *vendor, const char *library, const char *name, const char *version)
{
	return ((vendor  && strcmp(c->vlnv.vendor, vendor))	||
		(library && strcmp(c->vlnv.library, library))	||
		(name    && strcmp(c->vlnv.name, name))		||
		(version && strcmp(c->vlnv.version, version))) ? 0 : 1;
}

int ip_vlnv_parse(struct ip *c, const char *vlnv)
{
	char *tmp = strdup(vlnv);

	c->vlnv.vendor  = strdup(strtok(tmp, ":"));
	c->vlnv.library = strdup(strtok(NULL, ":"));
	c->vlnv.name    = strdup(strtok(NULL, ":"));
	c->vlnv.version = strdup(strtok(NULL, ":"));
	
	free(tmp);

	return 0;
}

int ip_init(struct ip *c)
{
	struct fpga *f = c->card;
	int ret;

	if      (ip_vlnv_match(c, "xilinx.com", "ip", "axis_interconnect", NULL)) {
		if (c != f->sw)
			error("There can be only one AXI4-Stream interconnect per FPGA");

		ret = switch_init(c);
	}
	else if (ip_vlnv_match(c, "xilinx.com", "ip", "axi_fifo_mm_s", NULL)) {
		ret = fifo_init(c);
	}
	else if (ip_vlnv_match(c, "xilinx.com", "ip", "axi_dma", NULL)) {
		ret = dma_init(c);
	}
	else if (ip_vlnv_match(c, "xilinx.com", "ip", "axi_timer", NULL)) {
		XTmrCtr_Config tmr_cfg = {
			.BaseAddress = (uintptr_t) c->card->map + c->baseaddr,
			.SysClockFreqHz = AXI_HZ
		};

		XTmrCtr_CfgInitialize(&c->timer, &tmr_cfg, (uintptr_t) c->card->map + c->baseaddr);
		XTmrCtr_InitHw(&c->timer);
	}
	else if (ip_vlnv_match(c, "xilinx.com", "ip", "axi_gpio", NULL)) {
		if (strstr(c->name, "reset")) {
			if (f->reset)
				error("There can be only one reset controller per FPGA");
			
			f->reset = c;
		}
	}
	else if (ip_vlnv_match(c, "acs.eonerc.rwth-aachen.de", "user", "axi_pcie_intc", NULL)) {
		if (c != f->intc)
			error("There can be only one interrupt controller per FPGA");

		ret = intc_init(c);
	}
	else if (ip_vlnv_match(c, NULL, "hls", NULL, NULL) ||
		 ip_vlnv_match(c, NULL, "sysgen", NULL, NULL)) {
		ret = model_init(c);
	}

	if (ret == 0)
		c->state = IP_STATE_INITIALIZED;

	return ret;
}

void ip_destroy(struct ip *c)
{
	if (ip_vlnv_match(c, NULL, "hls", NULL, NULL) ||
	    ip_vlnv_match(c, NULL, "sysgen", NULL, NULL))
		model_destroy(c);

	free(c->vlnv.vendor);
	free(c->vlnv.library);
	free(c->vlnv.name);
	free(c->vlnv.version);
}

void ip_dump(struct ip *c)
{
	info("IP %s: vlnv=%s:%s:%s:%s baseaddr=%#jx, irq=%d, port=%d",
		c->name, c->vlnv.vendor, c->vlnv.library, c->vlnv.name, c->vlnv.version, 
		c->baseaddr, c->irq, c->port);

	if (ip_vlnv_match(c, NULL, "hls", NULL, NULL) ||
	    ip_vlnv_match(c, NULL, "sysgen", NULL, NULL))
		model_dump(c);
}

int ip_parse(struct ip *c, config_setting_t *cfg)
{
	const char *vlnv;
	long long baseaddr;

	c->name = config_setting_name(cfg);
	if (!c->name)
		cerror(cfg, "IP is missing a name");

	if (!config_setting_lookup_string(cfg, "vlnv", &vlnv))
		cerror(cfg, "IP %s is missing the VLNV identifier", c->name);

	ip_vlnv_parse(c, vlnv);
	
	if      (ip_vlnv_match(c, "xilinx.com", "ip", "axis_interconnect", NULL)) {
		int numports;

		if (!config_setting_lookup_int(cfg, "numports", &numports))
			cerror(cfg, "Switch IP '%s' requires 'numports' option", c->name);
		
		c->sw.Config.MaxNumSI = c->sw.Config.MaxNumMI = numports;
	}
	else if (ip_vlnv_match(c, "xilinx.com", "ip", "axi_bram_ctrl", NULL)) {
		if (!config_setting_lookup_int(cfg, "size", &c->bram.size))
			cerror(cfg, "Block RAM requires 'size' option");
	}
	else if (ip_vlnv_match(c, "xilinx.com", "ip", "axi_fifo_mm_s", NULL)) {
		if (config_setting_lookup_int64(cfg, "baseaddr_axi4", &baseaddr))
			c->baseaddr_axi4 = baseaddr;
		else
			c->baseaddr_axi4 = -1;
	}
	else if (ip_vlnv_match(c, NULL, "hls", NULL, NULL) ||
		 ip_vlnv_match(c, NULL, "sysgen", NULL, NULL)) {

		if      (strcmp(c->vlnv.library, "hls") == 0)
			c->model.type = MODEL_TYPE_HLS;
		else if (strcmp(c->vlnv.library, "sysgen") == 0)
			c->model.type = MODEL_TYPE_XSG;
		else
			cerror(cfg, "Invalid model type: %s", c->vlnv.library);

		if (!config_setting_lookup_string(cfg, "xml", (const char **) &c->model.xml))
			c->model.xml = NULL;
	}

	/* Common settings */
	if (config_setting_lookup_int64(cfg, "baseaddr", &baseaddr))
		c->baseaddr = baseaddr;
	else
		c->baseaddr = -1;

	if (!config_setting_lookup_int(cfg, "irq", &c->irq))
		c->irq = -1;
	if (!config_setting_lookup_int(cfg, "port", &c->port))
		c->port = -1;

	c->cfg = cfg;

	return 0;
}
