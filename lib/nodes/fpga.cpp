/** Communicate with VILLASfpga Xilinx FPGA boards
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node_compat.hpp>
#include <villas/nodes/fpga.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>

#include <villas/fpga/core.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

/* Forward declartions */
static struct NodeCompatType p;

/* Global state */
static fpga::PCIeCard::List cards;
static std::map<fpga::ip::Dma, struct fpga_node *> dmaMap;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static std::shared_ptr<kernel::vfio::Container> vfioContainer;

using namespace villas;
using namespace villas::node;

int villas::node::fpga_type_start(SuperNode *sn)
{
	vfioContainer = kernel::vfio::Container::create();
	pciDevices = std::make_shared<kernel::pci::DeviceList>();

	// get the FPGA card plugin
	auto pcieCardPlugin = plugin::registry->lookup<fpga::PCIeCardFactory>("pcie");
	if (!pcieCardPlugin)
		throw RuntimeError("No FPGA PCIe plugin found");

	json_t *json = sn->getConfig();
	json_t *fpgas = json_object_get(json, "fpgas");
	if (!fpgas)
		throw ConfigError(json, "node-config-fpgas", "No section 'fpgas' found in config");

	// create all FPGA card instances using the corresponding plugin
	auto pcieCards = pcieCardPlugin->make(fpgas, pciDevices, vfioContainer);

	cards.splice(cards.end(), pcieCards);

	return 0;
}

int villas::node::fpga_type_stop()
{
	vfioContainer.reset(); // TODO: is this the proper way?

	return 0;
}

int villas::node::fpga_init(NodeCompat *n)
{
	auto *f = n->getData<struct fpga_node>();

	f->coalesce = 0;
	f->irqFd = -1;
	f->polling = true;

	new (&f->cardName) std::string();
	new (&f->dmaName) std::string();
	new (&f->intfName) std::string();

	new (&f->card) std::shared_ptr<fpga::PCIeCard>();

	new (&f->dma) std::shared_ptr<fpga::ip::Node>();
	new (&f->intf) std::shared_ptr<fpga::ip::Node>();

	new (&f->in.block) std::unique_ptr<MemoryBlock>();
	new (&f->out.block) std::unique_ptr<MemoryBlock>();

	return 0;
}

int villas::node::fpga_destroy(NodeCompat *n)
{
	auto *f = n->getData<struct fpga_node>();

	// using maiptr = MemoryAccessor<uint32_t>;
	// using mafptr = MemoryAccessor<float>;
	using mbptr = MemoryBlock::Ptr;
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

	f->in.block.~mbptr();
	f->out.block.~mbptr();

	return 0;
}

int villas::node::fpga_parse(NodeCompat *n, json_t *json)
{
	int ret;
	auto *f = n->getData<struct fpga_node>();

	json_error_t err;

	const char *card = nullptr;
	const char *intf = nullptr;
	const char *dma = nullptr;
	int polling = f->polling;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: s, s?: i, s?: b }",
		"card", &card,
		"interface", &intf,
		"dma", &dma,
		"coalesce", &f->coalesce,
		"polling", &polling
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-fpga");

	if (card)
		f->cardName = card;

	if (intf)
		f->intfName = intf;

	if (dma)
		f->dmaName = dma;

	f->polling = polling; // cast int to bool

	return 0;
}

char * villas::node::fpga_print(NodeCompat *n)
{
	auto *f = n->getData<struct fpga_node>();

	return strf("fpga=%s, dma=%s, if=%s, polling=%s, coalesce=%d",
		f->card->name.c_str(),
		f->dma->getInstanceName().c_str(),
		f->polling ? "yes" : "no",
		f->coalesce
	);
}

int villas::node::fpga_prepare(NodeCompat *n)
{
	int ret;
	auto *f = n->getData<struct fpga_node>();

	// Select first FPGA card
	auto it = f->cardName.empty()
		? cards.begin()
		: std::find_if(cards.begin(), cards.end(), [f](const fpga::PCIeCard::Ptr &c) {
			return c->name == f->cardName;
		});

	if (it == cards.end())
		throw ConfigError(json_object_get(n->getConfig(), "fpga"), "node-config-node-fpga-card", "Invalid FPGA card name: {}", f->cardName);

	f->card = *it;

	// Select interface IP core
	auto intf = f->intfName.empty()
		? f->card->lookupIp(fpga::Vlnv(FPGA_AURORA_VLNV))
		: f->card->lookupIp(f->intfName);
	if (!intf)
		throw ConfigError(n->getConfig(), "node-config-node-fpga-interface", "There is no interface IP with the name: {}", f->intfName);

	f->intf = std::dynamic_pointer_cast<fpga::ip::Node>(intf);
	if (!f->intf)
		throw RuntimeError("The IP {} is not a interface", *intf);

	// Select DMA IP core
	auto dma = f->dmaName.empty()
		? f->card->lookupIp(fpga::Vlnv(FPGA_DMA_VLNV))
		: f->card->lookupIp(f->dmaName);
	if (!dma)
		throw ConfigError(n->getConfig(), "node-config-node-fpga-dma", "There is no DMA IP with the name: {}", f->dmaName);

	f->dma = std::dynamic_pointer_cast<fpga::ip::Dma>(dma);
	if (!f->dma)
		throw RuntimeError("The IP {} is not a DMA controller", *dma);

	ret = f->intf->connect(*(f->dma), true);
	if (ret)
		throw RuntimeError("Failed to connect: {} -> {}",
			*(f->intf), *(f->dma)
		);

	auto &alloc = HostDmaRam::getAllocator();

	f->in.block  = std::move(alloc.allocateBlock(0x100 / sizeof(int32_t)));
	f->out.block = std::move(alloc.allocateBlock(0x100 / sizeof(int32_t)));

	f->in.accessor.i = MemoryAccessor<int32_t>(*f->in.block);
	f->in.accessor.f = MemoryAccessor<float>(*f->in.block);

	f->out.accessor.i = MemoryAccessor<int32_t>(*f->out.block);
	f->out.accessor.f = MemoryAccessor<float>(*f->out.block);

	f->dma->makeAccesibleFromVA(*f->in.block);
	f->dma->makeAccesibleFromVA(*f->out.block);

	f->dma->dump();
	f->intf->dump();
	MemoryManager::get().getGraph().dump();

	return 0;
}

int villas::node::fpga_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	unsigned read;
	auto *f = n->getData<struct fpga_node>();
	struct Sample *smp = smps[0];

	assert(cnt == 1);

	f->dma->read(*f->in.block.get(), f->in.block->getSize()); // TODO: calc size
	const size_t bytesRead = f->dma->readComplete();
	read = bytesRead / sizeof(int32_t);

	for (unsigned i = 0; i < MIN(read, smp->capacity); i++) {
		auto sig = n->getInputSignals(false)->getByIndex(i);

		switch (sig->type) {
			case SignalType::INTEGER:
				smp->data[i].i = f->in.accessor.i[i];
				break;

			case SignalType::FLOAT:
				smp->data[i].f = f->in.accessor.f[i];
				break;

			default: {}
		}
	}

	smp->signals = n->getInputSignals(false);
	smp->length = bytesRead / sizeof(uint32_t);
	smp->flags = (int) SampleFlags::HAS_DATA;

	return read;
}

int villas::node::fpga_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int written;
	auto *f = n->getData<struct fpga_node>();
	struct Sample *smp = smps[0];

	assert(cnt == 1);

	for (unsigned i = 0; i < smps[0]->length; i++) {
		auto sig = smp->signals->getByIndex(i);

		switch (sig->type) {
			case SignalType::INTEGER:
				f->out.accessor.i[i] = smps[0]->data[i].i;
				break;

			case SignalType::FLOAT:
				f->out.accessor.f[i] = smps[0]->data[i].f;
				break;

			default: {}
		}
	}

	bool state = f->dma->write(*f->out.block.get(), smp->length * sizeof(int32_t));
	if (!state)
		return -1;

	written = 0; /* The number of samples written */

	return written;
}

int villas::node::fpga_poll_fds(NodeCompat *n, int fds[])
{
	auto *f = n->getData<struct fpga_node>();

	if (f->polling)
		return 0;
	else {
		fds[0] = f->irqFd;

		return 1; /* The number of file descriptors which have been set in fds */
	}
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "fpga";
	p.description	= "Communicate with VILLASfpga Xilinx FPGA boards";
	p.vectorize	= 1;
	p.size		= sizeof(struct fpga_node);
	p.type.start	= fpga_type_start;
	p.type.stop	= fpga_type_stop;
	p.init		= fpga_init;
	p.destroy	= fpga_destroy;
	p.prepare	= fpga_prepare;
	p.parse		= fpga_parse;
	p.print		= fpga_print;
	p.read		= fpga_read;
	p.write		= fpga_write;
	p.poll_fds	= fpga_poll_fds;

	static NodeCompatFactory ncp(&p);
}

