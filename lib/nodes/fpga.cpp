/** Communicate with VILLASfpga Xilinx FPGA boards
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <csignal>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <jansson.h>

#include <villas/nodes/fpga.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/super_node.hpp>

#include <villas/fpga/core.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>

/* Forward declartions */
static struct plugin p;

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

/* Global state */
static fpga::PCIeCard::List cards;
static std::map<fpga::ip::Dma, struct fpga *> dmaMap;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static std::shared_ptr<kernel::vfio::Container> vfioContainer;

using namespace villas;

// static std::shared_ptr<fpga::ip::Dma>
// fpga_find_dma(const fpga::PCIeCard &card)
// {
// 	// for (auto &ip : card.ips) {

// 	// }

// 	return nullptr;
// }

int fpga_type_start(node::SuperNode *sn)
{
	vfioContainer = kernel::vfio::Container::create();
	pciDevices = std::make_shared<kernel::pci::DeviceList>();

	// get the FPGA card plugin
	auto pcieCardPlugin = plugin::Registry::lookup<fpga::PCIeCardFactory>("pcie");
	if (!pcieCardPlugin)
		throw RuntimeError("No FPGA PCIe plugin found");

	json_t *cfg = sn->getConfig();
	json_t *fpgas = json_object_get(cfg, "fpgas");
	if (!fpgas)
		throw ConfigError(cfg, "node-config-fpgas", "No section 'fpgas' found in config");

	// create all FPGA card instances using the corresponding plugin
	auto pcieCards = pcieCardPlugin->make(fpgas, pciDevices, vfioContainer);

	cards.splice(cards.end(), pcieCards);

	return 0;
}

int fpga_type_stop()
{
	vfioContainer.reset(); // TODO: is this the proper way?

	return 0;
}

int fpga_init(struct vnode *n)
{
	struct fpga *f = (struct fpga *) n->_vd;

	f->coalesce = 0;
	f->irqFd = -1;
	f->polling = true;

	new (&f->cardName) std::string();
	new (&f->dmaName) std::string();
	new (&f->intfName) std::string();

	new (&f->card) std::shared_ptr<fpga::PCIeCard>();

	new (&f->dma) std::shared_ptr<fpga::ip::Node>();
	new (&f->intf) std::shared_ptr<fpga::ip::Node>();

	// TODO: fixme
	// new (&f->in.mem) std::shared_ptr<MemoryBlock>();
	// new (&f->out.mem) std::shared_ptr<MemoryBlock>();

	return 0;
}

int fpga_destroy(struct vnode *n)
{
	struct fpga *f = (struct fpga *) n->_vd;

	using mbptr = MemoryAccessor<uint32_t>;
	using cptr = std::shared_ptr<fpga::PCIeCard>;
	using nptr = std::shared_ptr<fpga::ip::Node>;
	using dptr = std::shared_ptr<fpga::ip::Dma>;
	using sptr = std::string;

	f->cardName.~sptr();
	f->dmaName.~sptr();
	f->intfName.~sptr();

	f->card.~cptr();

	f->dma.~dptr();
	f->intf.~nptr();

	f->in.mem.~mbptr();
	f->out.mem.~mbptr();

	return 0;
}

int fpga_parse(struct vnode *n, json_t *cfg)
{
	int ret;
	struct fpga *f = (struct fpga *) n->_vd;

	json_error_t err;

	const char *card = nullptr;
	const char *intf = nullptr;
	const char *dma = nullptr;
	int polling = f->polling;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s?: i, s?: b }",
		"card", &card,
		"interface", &intf,
		"dma", &dma,
		"coalesce", &f->coalesce,
		"polling", &polling
	);
	if (ret)
		throw ConfigError(cfg, err, "node-config-fpga", "Failed to parse configuration of node {}", node_name(n));

	if (card)
		f->cardName = card;

	if (intf)
		f->intfName = intf;

	if (dma)
		f->dmaName = dma;

	f->polling = polling; // cast int to bool

	return 0;
}

char * fpga_print(struct vnode *n)
{
	struct fpga *f = (struct fpga *) n->_vd;

	return strf("fpga=%s, dma=%s, if=%s, polling=%s, coalesce=%d",
		f->card->name.c_str(),
		f->dma->getInstanceName().c_str(),
		f->polling ? "yes" : "no",
		f->coalesce
	);
}

int fpga_check(struct vnode *n)
{
	// struct fpga *f = (struct fpga *) n->_vd;

	return 0;
}

int fpga_prepare(struct vnode *n)
{
	int ret;
	struct fpga *f = (struct fpga *) n->_vd;

	auto it = f->cardName.empty()
		? cards.begin()
		: std::find_if(cards.begin(), cards.end(), [f](const fpga::PCIeCard::Ptr &c) {
			return c->name == f->cardName;
		});

	if (it == cards.end())
		throw ConfigError(json_object_get(n->cfg, "fpga"), "node-config-fpga-card", "Invalid FPGA card name: {}", f->cardName);

	f->card = *it;

	auto intf = f->intfName.empty()
		? f->card->lookupIp(fpga::Vlnv(FPGA_AURORA_VLNV))
		: f->card->lookupIp(f->intfName);
	if (!intf)
		throw ConfigError(n->cfg, "node-config-fpga-interface", "There is no interface IP with the name: {}", f->intfName);

	f->intf = std::dynamic_pointer_cast<fpga::ip::Node>(intf);
	if (!f->intf)
		throw RuntimeError("The IP {} is not a interface", *intf);

	auto dma = f->dmaName.empty()
		? f->card->lookupIp(fpga::Vlnv(FPGA_DMA_VLNV))
		: f->card->lookupIp(f->dmaName);
	if (!dma)
		throw ConfigError(n->cfg, "node-config-fpga-dma", "There is no DMA IP with the name: {}", f->dmaName);

	f->dma = std::dynamic_pointer_cast<fpga::ip::Dma>(dma);
	if (!f->dma)
		throw RuntimeError("The IP {} is not a DMA controller", *dma);

	ret = f->intf->connect(*(f->dma), true);
	if (ret)
		throw RuntimeError("Failed to connect: {} -> {}",
			*(f->intf), *(f->dma)
		);

	auto &alloc = HostRam::getAllocator();

	f->in.mem  = alloc.allocate<uint32_t>(0x100 / sizeof(int32_t));
	f->out.mem = alloc.allocate<uint32_t>(0x100 / sizeof(int32_t));

	f->in.block  = f->in.mem.getMemoryBlock();
	f->out.block = f->out.mem.getMemoryBlock();

	f->dma->makeAccesibleFromVA(f->in.block);
	f->dma->makeAccesibleFromVA(f->out.block);

	f->dma->dump();
	f->intf->dump();
	MemoryManager::get().getGraph().dump();

	return 0;
}

int fpga_start(struct vnode *n)
{
	// struct fpga *f = (struct fpga *) n->_vd;

	return 0;
}

int fpga_stop(struct vnode *n)
{
	//struct fpga *f = (struct fpga *) n->_vd;

	return 0;
}

int fpga_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	unsigned read;
	struct fpga *f = (struct fpga *) n->_vd;
	struct sample *smp = smps[0];

	assert(cnt == 1);

	f->dma->read(f->in.block, f->in.block.getSize()); // TODO: calc size
	const size_t bytesRead = f->dma->readComplete();
	read = bytesRead / sizeof(int32_t);

	for (unsigned i = 0; i < MIN(read, smp->capacity); i++)
		smp->data[i].i = f->in.mem[i];

	smp->signals = &n->in.signals;

	return read;
}

int fpga_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int written;
	struct fpga *f = (struct fpga *) n->_vd;
	struct sample *smp = smps[0];

	assert(cnt == 1);

	for (unsigned i = 0; i < smps[0]->length; i++)
		f->out.mem[i] = smps[0]->data[i].i;

	bool state = f->dma->write(f->out.block, smp->length * sizeof(int32_t));
	if (!state)
		return -1;

	written = 0; /* The number of samples written */

	return written;
}

int fpga_poll_fds(struct vnode *n, int fds[])
{
	struct fpga *f = (struct fpga *) n->_vd;

	if (f->polling)
		return 0;
	else {
		fds[0] = f->irqFd;

		return 1; /* The number of file descriptors which have been set in fds */
	}
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "fpga";
	p.description		= "Communicate with VILLASfpga Xilinx FPGA boards";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 1;
	p.node.size		= sizeof(struct fpga);
	p.node.type.start	= fpga_type_start;
	p.node.type.stop	= fpga_type_stop;
	p.node.init		= fpga_init;
	p.node.destroy		= fpga_destroy;
	p.node.prepare		= fpga_prepare;
	p.node.parse		= fpga_parse;
	p.node.print		= fpga_print;
	p.node.check		= fpga_check;
	p.node.start		= fpga_start;
	p.node.stop		= fpga_stop;
	p.node.read		= fpga_read;
	p.node.write		= fpga_write;
	p.node.poll_fds		= fpga_poll_fds;

	int ret = vlist_init(&p.node.instances);
	if (!ret)
		vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
