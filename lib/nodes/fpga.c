/** Node type: VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 *********************************************************************************/

#include <stdio.h>
#include <inttypes.h>

#include "kernel/kernel.h"
#include "kernel/pci.h"

#include "nodes/fpga.h"

#include "config.h"
#include "utils.h"
#include "timing.h"
#include "plugin.h"

#include "fpga/card.h"

static struct list cards;
static struct pci pci;
static struct vfio_container vc;

void fpga_dump(struct fpga *f)
{
	struct fpga_card *c = f->ip->card;
	
	fpga_card_dump(c);
}

int fpga_parse_cards(config_setting_t *cfg)
{
	int ret;
	config_setting_t *cfg_fpgas;

	cfg_fpgas = config_setting_get_member(cfg, "fpgas");
	if (!cfg_fpgas)
		cerror(cfg, "Config file is missing 'fpgas' section");
	
	if (!config_setting_is_group(cfg_fpgas))
		cerror(cfg_fpgas, "FPGA configuration section must be a group");
	
	for (int i = 0; i < config_setting_length(cfg_fpgas); i++) {
		config_setting_t *cfg_fpga = config_setting_get_elem(cfg_fpgas, i);
		
		struct fpga_card *c = alloc(sizeof(struct fpga_card));
		
		ret = fpga_card_parse(c, cfg_fpga);
		if (ret)
			cerror(cfg_fpga, "Failed to parse FPGA card configuration");
		
		list_push(&cards, c);
	}

	return 0;
}

int fpga_init(int argc, char *argv[], config_setting_t *cfg)
{
	int ret;
	
	ret = pci_init(&pci);
	if (ret)
		cerror(cfg, "Failed to initialize PCI sub-system");
	
	ret = vfio_init(&vc);
	if (ret)
		cerror(cfg, "Failed to initiliaze VFIO sub-system");

	/* Parse FPGA configuration */
	ret = fpga_parse_cards(cfg);
	if (ret)
		cerror(cfg, "Failed to parse VILLASfpga config");

	return 0;
}

int fpga_deinit()
{
	int ret;

	list_destroy(&cards, (dtor_cb_t) fpga_card_destroy, true);
	
	pci_destroy(&pci);

	ret = vfio_destroy(&vc);
	if (ret)
		error("Failed to deinitialize VFIO sub-system");
	
	ret = pci_destroy(&pci);
	if (ret)
		error("Failed to deinitialize PCI sub-system");

	return 0;
}

int fpga_parse(struct node *n, config_setting_t *cfg)
{
	struct fpga *f = n->_vd;
	struct fpga_card *card;
	struct fpga_ip *ip;

	char *cpy, *card_name, *ip_name;
	const char *dm;

	if (!config_setting_lookup_string(cfg, "datamover", &dm))
		cerror(cfg, "Node '%s' is missing the 'datamover' setting", node_name(n));

	if (!config_setting_lookup_bool(cfg, "use_irqs", &f->use_irqs))
		f->use_irqs = false;

	cpy = strdup(dm); /* strtok can not operate on type const char * */
	
	card_name = strtok(cpy, ":");
	ip_name = strtok(NULL, ":");

	card = list_lookup(&cards, card_name);
	if (!card)
		cerror(cfg, "There is no FPGA card named '%s", card_name);
	
	ip = list_lookup(&card->ips, ip_name);
	if (!ip)
		cerror(cfg, "There is no datamover named '%s' on the FPGA card '%s'", ip_name, card_name);
	if (!ip->_vt->type != FPGA_IP_TYPE_DATAMOVER)
		cerror(cfg, "The IP '%s' on FPGA card '%s' is not a datamover", ip_name, card_name);

	free(cpy);
	
	f->ip = ip;

	return 0;
}

char * fpga_print(struct node *n)
{
	struct fpga *f = n->_vd;
	struct fpga_card *c = f->ip->card;
	
	if (f->ip)
		return strf("dm=%s (%s:%s:%s:%s) baseaddr=%#jx port=%u slot=%02"PRIx8":%02"PRIx8".%"PRIx8" id=%04"PRIx16":%04"PRIx16,
			f->ip->name, f->ip->vlnv.vendor, f->ip->vlnv.library, f->ip->vlnv.name, f->ip->vlnv.version,
			f->ip->baseaddr, f->ip->port,
			c->filter.slot.bus, c->filter.slot.device, c->filter.slot.function,
			c->filter.id.vendor, c->filter.id.device);
	else
		return strf("dm=%s", f->ip->name);
}

int fpga_get_type(struct fpga_ip *c)
{
	if      (!fpga_vlnv_cmp(&c->vlnv, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL }))
		return FPGA_DM_DMA;
	else if (!fpga_vlnv_cmp(&c->vlnv, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_fifo_mm_s", NULL }))
		return FPGA_DM_FIFO;
	else
		return -1;
}

int fpga_start(struct node *n)
{
	int ret;

	struct fpga *f = n->_vd;
	struct fpga_card *c = f->ip->card;

	fpga_card_init(c, &pci, &vc);

	int flags = 0;
	if (!f->use_irqs)
		flags |= INTC_POLLING;

	switch (f->type) {
		case FPGA_DM_DMA:
			/* Map DMA accessible memory */
			ret = dma_alloc(f->ip, &f->dma, 0x1000, 0);
			if (ret)
				return ret;

			intc_enable(c->intc, (1 << (f->ip->irq    )), flags); /* MM2S */
			intc_enable(c->intc, (1 << (f->ip->irq + 1)), flags); /* S2MM */
			break;

		case FPGA_DM_FIFO:
			intc_enable(c->intc, (1 << f->ip->irq),      flags);	/* MM2S & S2MM */
			break;
	}
	

	return 0;
}

int fpga_stop(struct node *n)
{
	int ret;

	struct fpga *f = n->_vd;
	struct fpga_card *c = f->ip->card;

	switch (f->type) {
		case FPGA_DM_DMA:
			intc_disable(c->intc, f->ip->irq);	/* MM2S */
			intc_disable(c->intc, f->ip->irq + 1);	/* S2MM */

			ret = dma_free(f->ip, &f->dma);
			if (ret)
				return ret;

		case FPGA_DM_FIFO:
			if (f->use_irqs)
				intc_disable(c->intc, f->ip->irq);	/* MM2S & S2MM */
	}

	return 0;
}

int fpga_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;

	struct fpga *f = n->_vd;
	struct sample *smp = smps[0];

	size_t recvlen;

	size_t len = SAMPLE_DATA_LEN(64);
	
	/* We dont get a sequence no from the FPGA. Lets fake it */
	smp->sequence = -1;
	smp->ts.origin = time_now();

	/* Read data from RTDS */
	switch (f->type) {
		case FPGA_DM_DMA:
			ret = dma_read(f->ip, f->dma.base_phys + 0x800, len);
			if (ret)
				return ret;
			
			ret = dma_read_complete(f->ip, NULL, &recvlen);
			if (ret)
				return ret;

			memcpy(smp->data, f->dma.base_virt + 0x800, recvlen);

			smp->length = recvlen / 4;
			return 1;
		case FPGA_DM_FIFO:
			recvlen = fifo_read(f->ip, (char *) smp->data, len);
			
			smp->length = recvlen / 4;
			return 1;
	}

	return -1;
}

int fpga_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct fpga *f = n->_vd;
	struct sample *smp = smps[0];

	size_t sentlen;
	size_t len = smp->length * sizeof(smp->data[0]);

	/* Send data to RTDS */
	switch (f->type) {
		case FPGA_DM_DMA:
			memcpy(f->dma.base_virt, smp->data, len);

			ret = dma_write(f->ip, f->dma.base_phys, len);
			if (ret)
				return ret;
			
			ret = dma_write_complete(f->ip, NULL, &sentlen);
			if (ret)
				return ret;

			//info("Sent %u bytes to FPGA", sentlen);

			return 1;
		case FPGA_DM_FIFO:
			sentlen = fifo_write(f->ip, (char *) smp->data, len);
			return sentlen / sizeof(smp->data[0]);
			break;
	}
	
	return -1;
}

struct fpga_card * fpga_lookup_card(const char *name)
{
	return (struct fpga_card *) list_lookup(&cards, name);
}

static struct plugin p = {
	.name		= "fpga",
	.description	= "VILLASfpga PCIe card (libxil)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.size		= sizeof(struct fpga),
		.vectorize	= 1,
		.parse		= fpga_parse,
		.print		= fpga_print,
		.start		= fpga_start,
		.stop		= fpga_stop,
		.read		= fpga_read,
		.write		= fpga_write,
		.init		= fpga_init,
		.deinit		= fpga_deinit,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)