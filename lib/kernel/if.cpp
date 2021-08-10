/** Interface related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdio>
#include <cstdlib>
#include <dirent.h>

#include <netlink/route/link.h>

#include <villas/node/config.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/cpuset.hpp>

#include <villas/kernel/if.hpp>
#include <villas/kernel/tc.hpp>
#include <villas/kernel/tc_netem.hpp>
#include <villas/kernel/nl.hpp>
#include <villas/kernel/kernel.hpp>

#include <villas/nodes/socket.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::kernel;

Interface::Interface(struct rtnl_link *link, int aff) :
	nl_link(link),
	tc_qdisc(nullptr),
	affinity(aff)
{
	logger = logging.get(fmt::format("kernel:if:{}", getName()));

	int n = getIRQs();
	if (n)
		logger->warn("Did not found any interrupts");

	logger->debug("Found {} IRQs", irqs.size());
}

Interface::~Interface()
{
	if (tc_qdisc)
		rtnl_qdisc_put(tc_qdisc);
}

int Interface::start()
{
	logger->info("Starting interface which is used by {} nodes", nodes.size());

	/* Set affinity for network interfaces (skip _loopback_ dev) */
	if (affinity)
		setAffinity(affinity);

	/* Assign fwmark's to nodes which have netem options */
	int ret, fwmark = 0;
	for (auto *n : nodes) {
		if (n->tc_qdisc && n->fwmark < 0)
			n->fwmark = 1 + fwmark++;
	}

	/* Abort if no node is using netem */
	if (fwmark == 0)
		return 0;

	if (getuid() != 0)
		throw RuntimeError("Network emulation requires super-user privileges!");

	/* Replace root qdisc */
	ret = tc::prio(this, &tc_qdisc, TC_HANDLE(1, 0), TC_H_ROOT, fwmark);
	if (ret)
		throw RuntimeError("Failed to setup priority queuing discipline: {}", nl_geterror(ret));

	/* Create netem qdisks and appropriate filter per netem node */
	for (auto *n : nodes) {
		if (n->tc_qdisc) {
			ret = tc::mark(this,  &n->tc_classifier, TC_HANDLE(1, n->fwmark), n->fwmark);
			if (ret)
				throw RuntimeError("Failed to setup FW mark classifier: {}", nl_geterror(ret));

			char *buf = tc::netem_print(n->tc_qdisc);
			logger->debug("Starting network emulation for FW mark {}: {}", n->fwmark, buf);
			free(buf);

			ret = tc::netem(this, &n->tc_qdisc, TC_HANDLE(0x1000+n->fwmark, 0), TC_HANDLE(1, n->fwmark));
			if (ret)
				throw RuntimeError("Failed to setup netem qdisc: {}", nl_geterror(ret));
		}
	}

	return 0;
}

int Interface::stop()
{
	logger->info("Stopping interface");

	if (affinity)
		setAffinity(-1L);

	if (tc_qdisc)
		tc::reset(this);

	return 0;
}

std::string Interface::getName() const
{
	auto str = rtnl_link_get_name(nl_link);

	return std::string(str);
}

Interface * Interface::getEgress(struct sockaddr *sa, SuperNode *sn)
{
	struct rtnl_link *link;

	Logger logger = logging.get("kernel:if");

	auto & interfaces = sn->getInterfaces();
	auto affinity = sn->getAffinity();

	/* Determine outgoing interface */
	link = nl::get_egress_link(sa);
	if (!link)
		throw RuntimeError("Failed to get interface for socket address '{}'", socket_print_addr(sa));

	/* Search of existing interface with correct ifindex */
	for (auto *i : interfaces) {
		if (rtnl_link_get_ifindex(i->nl_link) == rtnl_link_get_ifindex(link))
			return i;
	}

	/* If not found, create a new interface */
	auto *i = new Interface(link, affinity);
	if (!i)
		throw MemoryAllocationError();

	interfaces.push_back(i);

	return i;
}

int Interface::getIRQs()
{
	int irq;

	auto dirname = fmt::format("/sys/class/net/{}/device/msi_irqs/", getName());
	DIR *dir = opendir(dirname.c_str());
	if (dir) {
		irqs.clear();

		struct dirent *entry;
		while ((entry = readdir(dir))) {
			irq = atoi(entry->d_name);
			if (irq)
				irqs.push_back(irq);
		}

		closedir(dir);
	}

	return 0;
}

int Interface::setAffinity(int affinity)
{
	assert(affinity != 0);

	if (getuid() != 0) {
		logger->warn("Failed to tune IRQ affinity. Please run as super-user");
		return 0;
	}

	FILE *file;

	CpuSet cset_pin(affinity);

	for (int irq : irqs) {
		std::string filename = fmt::format("/proc/irq/{}/smp_affinity", irq);

		file = fopen(filename.c_str(), "w");
		if (file) {
			if (fprintf(file, "%8lx", (unsigned long) cset_pin) < 0)
				throw SystemError("Failed to set affinity for for IRQ {} on interface '{}'", irq, getName());

			fclose(file);
			logger->debug("Set affinity of IRQ {} to {} {}", irq, cset_pin.count() == 1 ? "core" : "cores", (std::string) cset_pin);
		}
		else
			throw SystemError("Failed to set affinity for for IRQ {} on interface '{}'", irq, getName());
	}

	return 0;
}
