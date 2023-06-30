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
#include <memory>

#include <jansson.h>

#include <villas/node_compat.hpp>
#include <villas/nodes/fpga.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/core.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::fpga;
using namespace villas::utils;

/* Global state */
static std::list<std::shared_ptr<fpga::PCIeCard>> cards;
static std::map<fpga::ip::Dma, FpgaNode *> dmaMap;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static std::shared_ptr<kernel::vfio::Container> vfioContainer;

using namespace villas;
using namespace villas::node;

FpgaNode::FpgaNode(const std::string &name) :
	Node(name),
	irqFd(-1),
	coalesce(0),
	polling(true)
{ }

FpgaNode::~FpgaNode()
{ }

int FpgaNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json);
	if (ret)
		return ret;

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
		throw ConfigError(json, err, "node-config-fpga", "Failed to parse configuration of node {}", this->getName());

	if (card)
		cardName = card;

	if (intf)
		intfName = intf;

	if (dma)
		dmaName = dma;

	polling = poll; // cast int to bool

	return 0;
}

const std::string & FpgaNode::getDetails()
{
	if (details.empty()) {
		auto &name = card ? card->name : cardName;

		details = fmt::format("fpga={}, dma={}, if={}, polling={}, coalesce={}",
			name,
			dma->getInstanceName(),
			polling ? "yes" : "no",
			coalesce
		);
	}

	return details;
}

int FpgaNode::check()
{
	return 0;
}

int FpgaNode::prepare()
{
	auto it = cardName.empty()
		? cards.begin()
		: std::find_if(cards.begin(), cards.end(), [this](std::shared_ptr<fpga::PCIeCard> c) {
			return c->name == cardName;
		});

	if (it == cards.end())
		throw ConfigError(json_object_get(config, "fpga"), "node-config-fpga-card", "Invalid FPGA card name: {}", cardName);

	card = *it;

	auto intfCore = intfName.empty()
		? card->lookupIp(fpga::Vlnv(FPGA_AURORA_VLNV))
		: card->lookupIp(intfName);
	if (!intfCore)
		throw ConfigError(config, "node-config-fpga-interface", "There is no interface IP with the name: {}", intfName);

	intf = std::dynamic_pointer_cast<fpga::ip::Node>(intfCore);
	if (!intf)
		throw RuntimeError("The IP {} is not a interface", *intfCore);

	auto dmaCore = dmaName.empty()
		? card->lookupIp(fpga::Vlnv(FPGA_DMA_VLNV))
		: card->lookupIp(dmaName);
	if (!dmaCore)
		throw ConfigError(config, "node-config-fpga-dma", "There is no DMA IP with the name: {}", dmaName);

	dma = std::dynamic_pointer_cast<fpga::ip::Dma>(dmaCore);
	if (!dma)
		throw RuntimeError("The IP {} is not a DMA controller", *dmaCore);

	int ret = intf->connect(*(dma), true);
	if (ret)
		throw RuntimeError("Failed to connect: {} -> {}", *(intf), *(dma));

	auto &alloc = HostDmaRam::getAllocator();

	auto memRx  = alloc.allocate<uint32_t>(0x100 / sizeof(int32_t));
	auto memTx = alloc.allocate<uint32_t>(0x100 / sizeof(int32_t));

	blockRx = std::unique_ptr<const MemoryBlock>(&memRx.getMemoryBlock());
	blockTx = std::unique_ptr<const MemoryBlock>(&memTx.getMemoryBlock());

	dma->makeAccesibleFromVA(*blockRx);
	dma->makeAccesibleFromVA(*blockTx);

	// Show some debugging infos
	auto &mm = MemoryManager::get();

	dma->dump();
	intf->dump();
	mm.getGraph().dump();

	return Node::prepare();
}

int FpgaNode::_read(Sample *smps[], unsigned cnt)
{
	unsigned read;
	Sample *smp = smps[0];

	assert(cnt == 1);

	dma->read(*blockRx, blockRx->getSize()); // TODO: calc size
	const size_t bytesRead = dma->readComplete();
	read = bytesRead / sizeof(int32_t);

	auto mem = MemoryAccessor<uint32_t>(*blockRx);

	for (unsigned i = 0; i < MIN(read, smp->capacity); i++)
		smp->data[i].i = mem[i];

	smp->signals = in.signals;

	return read;
}

int FpgaNode::_write(Sample *smps[], unsigned cnt)
{
	int written;
	Sample *smp = smps[0];

	assert(cnt == 1);

	auto mem = MemoryAccessor<uint32_t>(*blockTx);

	for (unsigned i = 0; i < smps[0]->length; i++)
		mem[i] = smps[0]->data[i].i;

	bool state = dma->write(*blockTx, smp->length * sizeof(int32_t));
	if (!state)
		return -1;

	written = 0; /* The number of samples written */

	return written;
}

std::vector<int> FpgaNode::getPollFDs()
{
	std::vector<int> fds;

	if (!polling)
		fds.push_back(irqFd);

	return fds;
}

int FpgaNodeFactory::start(SuperNode *sn)
{
	vfioContainer = std::make_shared<kernel::vfio::Container>();
	pciDevices = std::make_shared<kernel::pci::DeviceList>();

	// Get the FPGA card plugin
	auto pcieCardPlugin = plugin::registry->lookup<fpga::PCIeCardFactory>("pcie");
	if (!pcieCardPlugin)
		throw RuntimeError("No FPGA PCIe plugin found");

	json_t *json_cfg = sn->getConfig();
	json_t *json_fpgas = json_object_get(json_cfg, "fpgas");
	if (!json_fpgas)
		throw ConfigError(json_cfg, "node-config-fpgas", "No section 'fpgas' found in config");

	// Create all FPGA card instances using the corresponding plugin
	auto piceCards = fpga::PCIeCardFactory::make(json_fpgas, pciDevices, vfioContainer);

	cards.splice(cards.end(), piceCards);

	return NodeFactory::start(sn);
}

static FpgaNodeFactory p;
