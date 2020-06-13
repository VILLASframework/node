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

#include <villas/nodes/villas_fpga.hpp>
#include <villas/log.hpp>
#include <villas/utils.h>
#include <villas/utils.hpp>
#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/super_node.hpp>

#include <villas/fpga/ip.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/rtds.hpp>

/* Forward declartions */
static struct plugin p;

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static struct pci pci;

int villas_fpga_parse_config()
{
	/* Parse FPGA configuration */
	FILE* f = fopen(configFile.c_str(), "r");
	if (f == nullptr) {
		logger->error("Cannot open config file: {}", configFile);
	}

	json_t* json = json_loadf(f, 0, nullptr);
	if (json == nullptr) {
		logger->error("Cannot parse JSON config");
		fclose(f);
		return -1;
	}

	fclose(f);

	json_t* fpgas = json_object_get(json, "fpgas");
	if (fpgas == nullptr) {
		logger->error("No section 'fpgas' found in config");
		exit(1);
	}

	return 0;
}

int villas_fpga_type_start(villas::node::SuperNode *sn)
{
	int ret;

	ret = pci_init(&pci);
	if (ret) {
		logger->error("Cannot initialize PCI subsystem");
		return ret;
	}

	auto vfioContainer = villas::VfioContainer::create();

	ret = villas_fpga_parse_config();
	if (ret)
		return ret;

	// get the FPGA card plugin
	villas::Plugin* plugin = villas::Plugin::lookup(villas::Plugin::Type::FpgaCard, "");
	if (plugin == nullptr) {
		logger->error("No FPGA plugin found");
		exit(1);
	}

	villas::fpga::PCIeCardFactory* fpgaCardPlugin =
	        dynamic_cast<villas::fpga::PCIeCardFactory*>(plugin);

	// create all FPGA card instances using the corresponding plugin
	auto cards = fpgaCardPlugin->make(fpgas, &pci, vfioContainer);

	return 0;
}

int villas_fpga_type_stop()
{

	return 0;
}

int villas_fpga_init(struct node *n)
{
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	return 0;
}

int villas_fpga_destroy(struct node *n)
{
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	return 0;
}

int villas_fpga_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	json_error_t err;

	const char *fpga_name;
	const char *dma_vlnv = "";
	const char *if_vlnv = "";

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: s, s?: s }",
		"fpga", &s->setting1,
		"", &s->setting2
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

char * villas_fpga_print(struct node *n)
{
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	return strf("fpga=%s, dma=%s, if=%s", f->fpga_name, f->dma_vlnv, f->);
}

int villas_fpga_check(struct node *n)
{
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	if (s->setting1 > 100 || s->setting1 < 0)
		return -1;

	if (!s->setting2 || strlen(s->setting2) > 10)
		return -1;

	return 0;
}

int villas_fpga_prepare(struct node *n)
{
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	for (auto& fpgaCard : cards) {
		if (fpgaCard->name == fpgaName) {
			f->card = fpgaCard;
			break;
		}
	}

	if (!card)
		logger->error("FPGA card {} not found in config or not working", fpgaName);

	// deallocate JSON config
	//json_decref(json);

	auto rtds = dynamic_cast<fpga::ip::Rtds*>
	            (card->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:user:rtds_axis:")));

	//auto dma = dynamic_cast<fpga::ip::Dma*>
	//          (card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));
	auto dma = dynamic_cast<fpga::ip::Dma*>
	          (card->lookupIp("hier_0_axi_dma_axi_dma_1"));

	if (!rtds) {
		logger->error("No RTDS interface found on FPGA");
		return 1;
	}

	if (!dma) {
		logger->error("No DMA found on FPGA ");
		return 1;
	}

	rtds->dump();

	rtds->connect(rtds->getMasterPort(rtds->masterPort),
	              dma->getSlavePort(dma->s2mmPort));

	dma->connect(dma->getMasterPort(dma->mm2sPort),
	             rtds->getSlavePort(rtds->slavePort));

	auto &alloc = villas::HostRam::getAllocator();

	auto mem = alloc.allocate<int32_t>(0x100 / sizeof(int32_t));
	auto block = mem.getMemoryBlock();

	dma->makeAccesibleFromVA(block);

	// auto &mm = MemoryManager::get();
	// mm.getMemoryGraph().dump("graph.dot");

	return 0;
}

int villas_fpga_start(struct node *n)
{
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	s->start_time = time_now();

	return 0;
}

int villas_fpga_stop(struct node *n)
{
	//struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int villas_fpga_pause(struct node *n)
{
	//struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int villas_fpga_resume(struct node *n)
{
	//struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int villas_fpga_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int read;
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;
	struct timespec now;

	dma->read(block, block.getSize());
	const size_t bytesRead = dma->readComplete();
	const size_t valuesRead = bytesRead / sizeof(int32_t);

	for (size_t i = 0; i < valuesRead; i++) {
		std::cerr << mem[i] << ";";
	}
	std::cerr << std::endl;

	return read;
}

int villas_fpga_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int written;
	struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	size_t memIdx = 0;

	for (unsigned i = 0; i < smps[0]->length; i++)
		f->tx_mem[memIdx++] = smps[0]->data[i].i;

	bool state = dma->write(block, memIdx * sizeof(int32_t));
	if (!state)
		logger->error("Failed to write to device");

	written = 0; /* The number of samples written */

	return written;
}

int villas_fpga_reverse(struct node *n)
{
	//struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int villas_fpga_poll_fds(struct node *n, int fds[])
{
	//struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

int villas_fpga_netem_fds(struct node *n, int fds[])
{
	//struct villas_fpga *f = (struct villas_fpga *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

__attribute__((constructor(110)))
static void register_plugin() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name			= "fpga";
	p.description		= "Communicate with VILLASfpga Xilinx FPGA boards";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct villas_fpga);
	p.node.type.start	= villas_fpga_type_start;
	p.node.type.stop	= villas_fpga_type_stop;
	p.node.init		= villas_fpga_init;
	p.node.destroy		= villas_fpga_destroy;
	p.node.prepare		= villas_fpga_prepare;
	p.node.parse		= villas_fpga_parse;
	p.node.print		= villas_fpga_print;
	p.node.check		= villas_fpga_check;
	p.node.start		= villas_fpga_start;
	p.node.stop		= villas_fpga_stop;
	p.node.pause		= villas_fpga_pause;
	p.node.resume		= villas_fpga_resume;
	p.node.read		= villas_fpga_read;
	p.node.write		= villas_fpga_write;
	p.node.reverse		= villas_fpga_reverse;
	p.node.poll_fds		= villas_fpga_poll_fds;
	p.node.netem_fds	= villas_fpga_netem_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
