/** FPGA card.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <unistd.h>

#include "config.h"

#include "kernel/pci.h"
#include "kernel/vfio.h"

#include "fpga/ip.h"
#include "fpga/card.h"

int fpga_card_init(struct fpga_card *c, struct pci *pci, struct vfio_container *vc)
{
	int ret;

	struct pci_dev *pdev;

	fpga_card_check(c);
	
	if (c->state == FPGA_CARD_STATE_INITIALIZED)
		return 0;

	/* Search for FPGA card */
	pdev = pci_lookup_device(pci, &c->filter);
	if (!pdev)
		error("Failed to find PCI device");

	/* Attach PCIe card to VFIO container */
	ret = vfio_pci_attach(&c->vd, vc, pdev);
	if (ret)
		error("Failed to attach VFIO device");

	/* Map PCIe BAR */
	c->map = vfio_map_region(&c->vd, VFIO_PCI_BAR0_REGION_INDEX);
	if (c->map == MAP_FAILED)
		serror("Failed to mmap() BAR0");

	/* Enable memory access and PCI bus mastering for DMA */
	ret = vfio_pci_enable(&c->vd);
	if (ret)
		serror("Failed to enable PCI device");
	
	/* Reset system? */
	if (c->do_reset) {
		/* Reset / detect PCI device */
		ret = vfio_pci_reset(&c->vd);
		if (ret)
			serror("Failed to reset PCI device");

		ret = fpga_card_reset(c);
		if (ret)
			error("Failed to reset FGPA card");
	}

	/* Initialize IP cores */
	list_foreach(struct fpga_ip *i, &c->ips) {
		ret = fpga_ip_init(i);
		if (ret)
			error("Failed to initalize IP core: %s (%u)", i->name, ret);
	}
	
	return 0;
}

int fpga_card_parse(struct fpga_card *c, config_setting_t *cfg)
{
	int ret;
	const char *slot, *id, *err;
	config_setting_t *cfg_ips, *cfg_slot, *cfg_id;

	/* Default values */
	c->filter.id.vendor = FPGA_PCI_VID_XILINX;
	c->filter.id.device = FPGA_PCI_PID_VFPGA;
	
	c->name = config_setting_name(cfg);
	c->state = FPGA_CARD_STATE_UNKOWN;
	
	if (!config_setting_lookup_int(cfg, "affinity", &c->affinity))
		c->affinity = 0;

	if (!config_setting_lookup_bool(cfg, "do_reset", &c->do_reset))
		c->do_reset = 0;

	cfg_slot = config_setting_get_member(cfg, "slot");
	if (cfg_slot) {
		slot = config_setting_get_string(cfg_slot);
		if (slot) {
			ret = pci_dev_parse_slot(&c->filter, slot, &err);
			if (ret)
				cerror(cfg_slot, "Failed to parse PCI slot: %s", err);
		}
		else
			cerror(cfg_slot, "PCI slot must be a string");
	}

	cfg_id = config_setting_get_member(cfg, "id");
	if (cfg_id) {
		id = config_setting_get_string(cfg_id);
		if (id) {
			ret = pci_dev_parse_id(&c->filter, (char*) id, &err);
			if (ret)
				cerror(cfg_id, "Failed to parse PCI id: %s", err);
		}
		else
			cerror(cfg_slot, "PCI ID must be a string");
	}
	
	cfg_ips = config_setting_get_member(cfg, "ips");
	if (!cfg_ips)
		cerror(cfg, "FPGA configuration is missing ips section");

	for (int i = 0; i < config_setting_length(cfg_ips); i++) {
		config_setting_t *cfg_ip = config_setting_get_elem(cfg_ips, i);

		struct fpga_ip ip = {
			.card = c
		};
	
		ret = fpga_ip_parse(&ip, cfg_ip);
		if (ret)
			cerror(cfg_ip, "Failed to parse VILLASfpga IP core");

		list_push(&c->ips, memdup(&ip, sizeof(ip)));
	}
	
	c->cfg = cfg;
	
	return 0;
}

int fpga_card_check(struct fpga_card *c)
{
	/* Check FPGA configuration */
	c->reset = fpga_vlnv_lookup(&c->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_gpio", NULL });
	if (!c->reset)
		error("FPGA is missing a reset controller");

	c->intc = fpga_vlnv_lookup(&c->ips, &(struct fpga_vlnv) { "acs.eonerc.rwth-aachen.de", "user", "axi_pcie_intc", NULL });
	if (!c->intc)
		error("FPGA is missing a interrupt controller");

	c->sw = fpga_vlnv_lookup(&c->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axis_interconnect", NULL });
	if (!c->sw)
		warn("FPGA is missing an AXI4-Stream switch");
	
	return 0;
}

int fpga_card_destroy(struct fpga_card *c)
{
	list_destroy(&c->ips, (dtor_cb_t) fpga_ip_destroy, true);
	
	return 0;
}

int fpga_card_reset(struct fpga_card *c)
{
	int ret;
	char state[4096];

	/* Save current state of PCI configuration space */
	ret = pread(c->vd.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	uint32_t *rst_reg = (uint32_t *) (c->map + c->reset->baseaddr);

	debug(3, "FPGA: reset");
	rst_reg[0] = 1;

	usleep(100000);

	/* Restore previous state of PCI configuration space */
	ret = pwrite(c->vd.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	/* After reset the value should be zero again */
	if (rst_reg[0])
		return -2;
	
	c->state = FPGA_CARD_STATE_RESETTED;

	return 0;
}