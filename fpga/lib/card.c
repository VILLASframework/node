/** FPGA card.
 *
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

#include <unistd.h>

#include "config.h"
#include "log.h"
#include "log_config.h"
#include "list.h"
#include "utils.h"

#include "kernel/pci.h"
#include "kernel/vfio.h"

#include "fpga/ip.h"
#include "fpga/card.h"

int fpga_card_init(struct fpga_card *c, struct pci *pci, struct vfio_container *vc)
{
	assert(c->state == STATE_DESTROYED);

	c->vfio_container = vc;
	c->pci = pci;

	list_init(&c->ips);

	/* Default values */
	c->filter.id.vendor = FPGA_PCI_VID_XILINX;
	c->filter.id.device = FPGA_PCI_PID_VFPGA;

	c->affinity = 0;
	c->do_reset = 0;

	c->state = STATE_INITIALIZED;

	return 0;
}

int fpga_card_parse(struct fpga_card *c, json_t *cfg, const char *name)
{
	int ret;

	json_t *json_ips;
	json_t *json_slot = NULL;
	json_t *json_id = NULL;
	json_error_t err;

	c->name = strdup(name);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: b, s?: o, s?: o, s: o }",
		"affinity", &c->affinity,
		"do_reset", &c->do_reset,
		"slot", &json_slot,
		"id", &json_id,
		"ips", &json_ips
	);
	if (ret)
		jerror(&err, "Failed to parse FPGA vard configuration");

	if (json_slot) {
		const char *err, *slot;

		slot = json_string_value(json_slot);
		if (slot) {
			ret = pci_device_parse_slot(&c->filter, slot, &err);
			if (ret)
				error("Failed to parse PCI slot: %s", err);
		}
		else
			error("PCI slot must be a string");
	}

	if (json_id) {
		const char *err, *id;

		id = json_string_value(json_id);
		if (id) {
			ret = pci_device_parse_id(&c->filter, (char*) id, &err);
			if (ret)
				error("Failed to parse PCI id: %s", err);
		}
		else
			error("PCI ID must be a string");
	}

	if (!json_is_object(json_ips))
		error("FPGA card IPs section must be an object");

	const char *name_ip;
	json_t *json_ip;
	json_object_foreach(json_ips, name_ip, json_ip) {
		const char *vlnv;

		struct fpga_ip_type *vt;
		struct fpga_ip *ip = (struct fpga_ip *) alloc(sizeof(struct fpga_ip));

		ip->card = c;

		ret = json_unpack_ex(json_ip, &err, 0, "{ s: s }", "vlnv", &vlnv);
		if (ret)
			error("Failed to parse FPGA IP '%s' of card '%s'", name_ip, name);

		vt = fpga_ip_type_lookup(vlnv);
		if (!vt)
			error("FPGA IP core VLNV identifier '%s' is invalid", vlnv);

		ret = fpga_ip_init(ip, vt);
		if (ret)
			error("Failed to initalize FPGA IP core");

		ret = fpga_ip_parse(ip, json_ip, name_ip);
		if (ret)
			error("Failed to parse FPGA IP core");

		list_push(&c->ips, ip);
	}

	c->state = STATE_PARSED;

	return 0;
}

int fpga_card_parse_list(struct list *cards, json_t *cfg)
{
	int ret;

	if (!json_is_object(cfg))
		error("FPGA card configuration section must be a JSON object");

	const char *name;
	json_t *json_fpga;
	json_object_foreach(cfg, name, json_fpga) {
		struct fpga_card *c = (struct fpga_card *) alloc(sizeof(struct fpga_card));

		ret = fpga_card_parse(c, json_fpga, name);
		if (ret)
			error("Failed to parse FPGA card configuration");

		list_push(cards, c);
	}

	return 0;
}

int fpga_card_start(struct fpga_card *c)
{
	int ret;

	struct pci_device *pdev;

	assert(c->state == STATE_CHECKED);

	/* Search for FPGA card */
	pdev = pci_lookup_device(c->pci, &c->filter);
	if (!pdev)
		error("Failed to find PCI device");

	/* Attach PCIe card to VFIO container */
	ret = vfio_pci_attach(&c->vfio_device, c->vfio_container, pdev);
	if (ret)
		error("Failed to attach VFIO device");

	/* Map PCIe BAR */
	c->map = vfio_map_region(&c->vfio_device, VFIO_PCI_BAR0_REGION_INDEX);
	if (c->map == MAP_FAILED)
		serror("Failed to mmap() BAR0");

	/* Enable memory access and PCI bus mastering for DMA */
	ret = vfio_pci_enable(&c->vfio_device);
	if (ret)
		serror("Failed to enable PCI device");

	/* Reset system? */
	if (c->do_reset) {
		/* Reset / detect PCI device */
		ret = vfio_pci_reset(&c->vfio_device);
		if (ret)
			serror("Failed to reset PCI device");

		ret = fpga_card_reset(c);
		if (ret)
			error("Failed to reset FGPA card");
	}

	/* Initialize IP cores */
	for (size_t j = 0; j < list_length(&c->ips); j++) {
		struct fpga_ip *i = (struct fpga_ip *) list_at(&c->ips, j);

		ret = fpga_ip_start(i);
		if (ret)
			error("Failed to initalize FPGA IP core: %s (%u)", i->name, ret);
	}

	c->state = STATE_STARTED;

	return 0;
}

int fpga_card_stop(struct fpga_card *c)
{
	int ret;

	assert(c->state == STATE_STOPPED);

	for (size_t j = 0; j < list_length(&c->ips); j++) {
		struct fpga_ip *i = (struct fpga_ip *) list_at(&c->ips, j);

		ret = fpga_ip_stop(i);
		if (ret)
			error("Failed to stop FPGA IP core: %s (%u)", i->name, ret);
	}

	c->state = STATE_STOPPED;

	return 0;
}

void fpga_card_dump(struct fpga_card *c)
{
	info("VILLASfpga card:");
	{ INDENT
		info("Slot: %04x:%02x:%02x.%d", c->vfio_device.pci_device->slot.domain, c->vfio_device.pci_device->slot.bus, c->vfio_device.pci_device->slot.device, c->vfio_device.pci_device->slot.function);
		info("Vendor ID: %04x", c->vfio_device.pci_device->id.vendor);
		info("Device ID: %04x", c->vfio_device.pci_device->id.device);
		info("Class  ID: %04x", c->vfio_device.pci_device->id.class);

		info("BAR0 mapped at %p", c->map);

		info("IP blocks:");
		for (size_t j = 0; j < list_length(&c->ips); j++) { INDENT
			struct fpga_ip *i = (struct fpga_ip *) list_at(&c->ips, j);

			fpga_ip_dump(i);
		}
	}

	vfio_dump(c->vfio_device.group->container);
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
	ret = pread(c->vfio_device.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	uint32_t *rst_reg = (uint32_t *) (c->map + c->reset->baseaddr);

	debug(3, "FPGA: reset");
	rst_reg[0] = 1;

	usleep(100000);

	/* Restore previous state of PCI configuration space */
	ret = pwrite(c->vfio_device.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	/* After reset the value should be zero again */
	if (rst_reg[0])
		return -2;

	c->state = STATE_INITIALIZED;

	return 0;
}
