/** Communicate with VILLASfpga Xilinx FPGA boards
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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

#include <villas/fpga/pcie_card.hpp>
#include <villas/fpga/core.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::fpga;
using namespace villas::utils;

/* Forward declartions */
static struct NodeCompatType p;

/* Global state */
static fpga::Card::List cards;
static std::map<fpga::ip::Dma, struct fpga *> dmaMap;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static std::shared_ptr<kernel::vfio::Container> vfioContainer;

using namespace villas;
using namespace villas::node;

FpgaNode::FpgaNode(struct vnode *n) :
	Node(n),
	irqFd(-1),
	coalesce(0),
	polling(true)
{ }

FpgaNode::~FpgaNode()
{ }

int FpgaNode::typeStart(node::SuperNode *sn)
{
	vfioContainer = kernel::vfio::Container::create();
	pciDevices = std::make_shared<kernel::pci::DeviceList>();

	// get the FPGA card plugin
	auto pcieCardPlugin = plugin::registry->lookup<fpga::PCIeCardFactory>("pcie");
	if (!pcieCardPlugin)
		throw RuntimeError("No FPGA PCIe plugin found");

	json_t *json_cfg = sn->getConfig();
	json_t *json_fpgas = json_object_get(json_cfg, "fpgas");
	if (!json_fpgas)
		throw ConfigError(json, "node-config-fpgas", "No section 'fpgas' found in config");

	// create all FPGA card instances using the corresponding plugin
	auto cards = CardFactory::make(json_fpgas, pciDevices, vfioContainer);

	cards.splice(cards.end(), cards);

	return 0;
}

int FpgaNode::typeStop()
{
	vfioContainer.reset(); // TODO: is this the proper way?

	return 0;
}

int FpgaNode::parse(json_t *cfg)
{
	int ret;

	json_error_t err;

	const char *card = nullptr;
	const char *intf = nullptr;
	const char *dma = nullptr;
	int poll = polling;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: s, s?: i, s?: b }",
		"card", &card,
		"interface", &intf,
		"dma", &dma,
		"coalesce", &coalesce,
		"polling", &polling
	);
	if (ret)
		throw ConfigError(cfg, err, "node-config-fpga", "Failed to parse configuration of node {}", node_name(node));

	if (card)
		cardName = card;

	if (intf)
		intfName = intf;

	if (dma)
		dmaName = dma;

	polling = poll; // cast int to bool

	return 0;
}

char * FpgaNode::print()
{
	auto &cardName = card ? card->name : cardName;

	return strf("fpga=%s, dma=%s, if=%s, polling=%s, coalesce=%d",
		cardName.c_str(),
		dma->getInstanceName().c_str(),
		polling ? "yes" : "no",
		coalesce
	);
}

int FpgaNode::check()
{
	return 0;
}

int FpgaNode::prepare()
{
	int ret;

	auto it = cardName.empty()
		? cards.begin()
		: std::find_if(cards.begin(), cards.end(), [this](const fpga::Card::Ptr &c) {
			return c->name == cardName;
		});

	if (it == cards.end())
		throw ConfigError(json_object_get(cfg, "fpga"), "node-config-fpga-card", "Invalid FPGA card name: {}", cardName);

	card = *it;

	auto intf = intfName.empty()
		? card->lookupIp(fpga::Vlnv(FPGA_AURORA_VLNV))
		: card->lookupIp(intfName);
	if (!intf)
		throw ConfigError(cfg, "node-config-fpga-interface", "There is no interface IP with the name: {}", intfName);

	intf = std::dynamic_pointer_cast<fpga::ip::Node>(intf);
	if (!intf)
		throw RuntimeError("The IP {} is not a interface", *intf);

	auto dma = dmaName.empty()
		? card->lookupIp(fpga::Vlnv(FPGA_DMA_VLNV))
		: card->lookupIp(dmaName);
	if (!dma)
		throw ConfigError(cfg, "node-config-fpga-dma", "There is no DMA IP with the name: {}", dmaName);

	dma = std::dynamic_pointer_cast<fpga::ip::Dma>(dma);
	if (!dma)
		throw RuntimeError("The IP {} is not a DMA controller", *dma);

	ret = intf->connect(*(dma), true);
	if (ret)
		throw RuntimeError("Failed to connect: {} -> {}",
			*(intf), *(dma)
		);

	auto &alloc = HostDmaRam::getAllocator();

	in.mem  = alloc.allocate<uint32_t>(0x100 / sizeof(int32_t));
	out.mem = alloc.allocate<uint32_t>(0x100 / sizeof(int32_t));

	in.block  = in.mem.getMemoryBlock();
	out.block = out.mem.getMemoryBlock();

	dma->makeAccesibleFromVA(in.block);
	dma->makeAccesibleFromVA(out.block);

	dma->dump();
	intf->dump();
	MemoryManager::get().getGraph().dump();

	return 0;
}

int FpgaNode::read(struct sample *smps[], unsigned cnt, unsigned *release)
{
	unsigned read;
		struct sample *smp = smps[0];

	assert(cnt == 1);

	dma->read(in.block, in.block.getSize()); // TODO: calc size
	const size_t bytesRead = dma->readComplete();
	read = bytesRead / sizeof(int32_t);

	for (unsigned i = 0; i < MIN(read, smp->capacity); i++)
		smp->data[i].i = in.mem[i];

	smp->signals = &node->in.signals;

	return read;
}

int FpgaNode::write(struct sample *smps[], unsigned cnt, unsigned *release)
{
	int written;
		struct sample *smp = smps[0];

	assert(cnt == 1);

	for (unsigned i = 0; i < smps[0]->length; i++)
		out.mem[i] = smps[0]->data[i].i;

	bool state = dma->write(out.block, smp->length * sizeof(int32_t));
	if (!state)
		return -1;

	written = 0; /* The number of samples written */

	return written;
}

int FpgaNode::pollFDs(int fds[])
{
	if (polling)
		return 0;
	else {
		fds[0] = irqFd;

		return 1; /* The number of file descriptors which have been set in fds */
	}
}

static char n[] = "fpga";
static char d[] = "VILLASfpga";
static NodePlugin<FpgaNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE | (int) NodeFactory::Flags::SUPPORTS_POLL> p;
