/** Node type: Ethercat
 *
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Divya Laxetti <divya.laxetti@rwth-aachen.de>
 * @copyright 2018-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <sstream>

#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/ethercat.hpp>

using namespace villas;
using namespace villas::node;

/* Forward declartions */
static NodeCompatType p;

/* Constants */
#define NSEC_PER_SEC (1000000000)
#define FREQUENCY (NSEC_PER_SEC / PERIOD_NS)

/* Global state and config */
int master_id = 0;
int alias = 0;

static ec_master_t *master = nullptr;

struct coupler {
	int position;
	int vendor_id;
	int product_code;

	ec_slave_config_t *sc;
} coupler = {
	.position = 0,
	.vendor_id = ETHERCAT_VID_BECKHOFF,
	.product_code = ETHERCAT_PID_EK1100,
	.sc = nullptr
};

static
void ethercat_cyclic_task(NodeCompat *n)
{
	int ret;
	struct Sample *smp;
	auto *w = n->getData<struct ethercat>();

	while (true) {
		w->task.wait();

		/* Receive process data */
		ecrt_master_receive(master);
		ecrt_domain_process(w->domain);

		/* Receive process data */
		smp = sample_alloc(&w->pool);
		if (!smp) {
			n->logger->warn("Pool underrun");
			continue;
		}

		smp->length = MIN(w->in.num_channels, smp->capacity);
		smp->flags = (int) SampleFlags::HAS_DATA;
		smp->signals = n->getInputSignals(false);

		/* Read process data */
		for (unsigned i = 0; i < smp->length; i++) {
			int16_t ain_value = EC_READ_S16(w->domain_pd + w->in.offsets[i]);

			smp->data[i].f = w->in.range * (float) ain_value / INT16_MAX;
		}

		ret = queue_signalled_push(&w->queue, smp);
		if (ret)
			n->logger->warn("Failed to enqueue samples");

		/* Write process data */
		smp = w->send.exchange(nullptr);

		for (unsigned i = 0; i < w->out.num_channels; i++) {
			int16_t aout_value = (smp->data[i].f / w->out.range) * INT16_MAX;

			EC_WRITE_S16(w->domain_pd + w->out.offsets[i], aout_value);
		}

		sample_decref(smp);

		// send process data
		ecrt_domain_queue(w->domain);
		ecrt_master_send(master);
	}
}

int villas::node::ethercat_type_start(villas::node::SuperNode *sn)
{
	int ret;
	json_error_t err;

	json_t *json = sn->getConfig();
	if (json) {
		ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?:i, s?: { s?: { s?: i, s?: i, s?: i } } }",
			"ethernet",
				"master", &master_id,
				"alias", &alias,
				"coupler",
					"position", &coupler.position,
					"product_code", &coupler.product_code,
					"vendor_id", &coupler.vendor_id
		);
		if (ret)
			throw ConfigError(json, err, "node-config-node-ethercat");
	}

	master = ecrt_request_master(master_id);
	if (!master)
		return -1;

	/* Create configuration for bus coupler */
	coupler.sc = ecrt_master_slave_config(master, alias, coupler.position, coupler.vendor_id, coupler.product_code);
	if (!coupler.sc)
		return -1;

	return 0;
}

int villas::node::ethercat_type_stop()
{
	auto logger = logging.get("node:ethercat");

	logger->info("Releasing EtherCAT master");

	ecrt_release_master(master);

	return 0;
}

int villas::node::ethercat_parse(NodeCompat *n, json_t *json)
{
	auto *w = n->getData<struct ethercat>();

	int ret;
	json_error_t err;

	ret = json_unpack_ex(json, &err, 0, "{ s?: F, s?: { s: i, s?: F, s?: i, s?: i, s?: i }, s?: { s: i, s?: F, s?: i, s?: i, s?: i } }",
		"rate", &w->rate,
		"out",
			"num_channels", &w->out.num_channels,
			"range", &w->out.range,
			"position", &w->out.position,
			"product_code", &w->out.product_code,
			"vendor_id", &w->out.vendor_id,
		"in",
			"num_channels", &w->in.num_channels,
			"range", &w->in.range,
			"position", &w->in.position,
			"product_code", &w->in.product_code,
			"vendor_id", &w->in.vendor_id
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-ethercat");

	return 0;
}

char * villas::node::ethercat_print(NodeCompat *n)
{
	auto *w = n->getData<struct ethercat>();
	std::stringstream ss;

	ss << "alias=" << alias;
	ss << ", in.range=" << w->in.range << ", in.num_channels=" << w->in.num_channels;
	ss << ", out.range=" << w->out.range << ", out.num_channels=" << w->out.num_channels;

	return strdup(ss.str().c_str());
}

int villas::node::ethercat_check(NodeCompat *n)
{
	auto *w = n->getData<struct ethercat>();

	/* Some parts of the configuration are still hard-coded for this specific setup */
	if (w->in.product_code != ETHERCAT_PID_EL3008 ||
		w->in.vendor_id != ETHERCAT_VID_BECKHOFF ||
		w->out.product_code != ETHERCAT_PID_EL4038 ||
		w->out.vendor_id != ETHERCAT_VID_BECKHOFF ||
		coupler.product_code != ETHERCAT_PID_EK1100 ||
		coupler.vendor_id != ETHERCAT_VID_BECKHOFF
	)
		return -1;

	return 0;
}

int villas::node::ethercat_prepare(NodeCompat *n)
{
	int ret;
	auto *w = n->getData<struct ethercat>();

	ret = pool_init(&w->pool, DEFAULT_ETHERCAT_QUEUE_LENGTH, SAMPLE_LENGTH(n->getInputSignals(false)->size()));
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_ETHERCAT_QUEUE_LENGTH);
	if (ret)
		return ret;

	w->in.offsets = new unsigned[w->in.num_channels];
	if (!w->in.offsets)
		throw MemoryAllocationError();

	w->out.offsets = new unsigned[w->out.num_channels];
	if (!w->out.offsets)
		throw MemoryAllocationError();

	w->domain_regs = new ec_pdo_entry_reg_t[w->in.num_channels + w->out.num_channels + 1];
	if (!w->domain_regs)
		throw MemoryAllocationError();

	memset(w->domain_regs, 0, (w->in.num_channels + w->out.num_channels + 1) * sizeof(ec_pdo_entry_reg_t));

	/* Prepare list of domain registers */
	int o = 0;
	for (unsigned i = 0; i < w->out.num_channels; i++) {
		w->out.offsets[i] = 0;

		w->domain_regs[o].alias = alias;
		w->domain_regs[o].position = w->out.position;
		w->domain_regs[o].vendor_id = w->out.vendor_id;
		w->domain_regs[o].product_code = w->out.product_code;
		w->domain_regs[o].index = 0x7000 + i * 0x10;
		w->domain_regs[o].subindex = 0x1;
		w->domain_regs[o].offset = w->out.offsets + i;

		o++;
	};

	/* Prepare list of domain registers */
	for (unsigned i = 0; i < w->in.num_channels; i++) {
		w->in.offsets[i] = 0;

		w->domain_regs[o].alias = alias;
		w->domain_regs[o].position = w->in.position;
		w->domain_regs[o].vendor_id = w->in.vendor_id;
		w->domain_regs[o].product_code = w->in.product_code;
		w->domain_regs[o].index = 0x6000 + i * 0x10;
		w->domain_regs[o].subindex = 0x11;
		w->domain_regs[o].offset = w->in.offsets + i;

		o++;
	};

	w->domain = ecrt_master_create_domain(master);
	if (!w->domain)
		return -1;

	return 0;
}

int villas::node::ethercat_start(NodeCompat *n)
{
	int ret;
	auto *w = n->getData<struct ethercat>();

	/* Configure analog in */
	w->in.sc = ecrt_master_slave_config(master, alias, w->in.position, w->in.vendor_id, w->in.product_code);
	if (!w->in.sc)
		throw RuntimeError("Failed to get slave configuration.");

	ret = ecrt_slave_config_pdos(w->in.sc, EC_END, slave_4_syncs);
	if (ret)
		throw RuntimeError("Failed to configure PDOs.");

	/* Configure analog out */
	w->out.sc = ecrt_master_slave_config(master, alias, w->out.position, w->out.vendor_id, w->out.product_code);
	if (!w->out.sc)
		throw RuntimeError("Failed to get slave configuration.");

	ret = ecrt_slave_config_pdos(w->out.sc, EC_END, slave_3_syncs);
	if (ret)
		throw RuntimeError("Failed to configure PDOs.");

	ret = ecrt_domain_reg_pdo_entry_list(w->domain, w->domain_regs);
	if (ret)
		throw RuntimeError("PDO entry registration failed!");

	/** @todo Check that master is not already active... */
	ret = ecrt_master_activate(master);
	if (ret)
		return -1;

	w->domain_pd = ecrt_domain_data(w->domain);
	if (!w->domain_pd)
		return -1;

	/* Start cyclic timer */
	w->task.setRate(w->rate);

	/* Start cyclic task */
	w->thread = std::thread(ethercat_cyclic_task, n);

	return 0;
}

int villas::node::ethercat_stop(NodeCompat *n)
{
	int ret;
	auto *w = n->getData<struct ethercat>();

	w->thread.join();

	ret = queue_signalled_close(&w->queue);
	if (ret)
		return ret;

	return 0;
}

int villas::node::ethercat_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *w = n->getData<struct ethercat>();

	int avail;
	struct Sample *cpys[cnt];

	avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
	if (avail < 0)
		n->logger->warn("Pool underrun: avail={}", avail);

	sample_copy_many(smps, cpys, avail);
	sample_decref_many(cpys, avail);

	return avail;
}

int villas::node::ethercat_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *w = n->getData<struct ethercat>();

	if (cnt < 1)
		return cnt;

	struct Sample *smp = smps[0];

	sample_incref(smp);

	struct Sample *old = w->send.exchange(smp);
	if (old)
		sample_decref(old);

	return 1;
}

int villas::node::ethercat_init(NodeCompat *n)
{
	auto *w = n->getData<struct ethercat>();

	/* Default values */
	w->rate = 1000;

	w->in.num_channels = 8;
	w->in.range = 10.0;
	w->in.position = 2;
	w->in.product_code = ETHERCAT_PID_EL3008;
	w->in.vendor_id = ETHERCAT_VID_BECKHOFF;
	w->in.sc = nullptr;
	w->in.offsets = nullptr;

	w->out.num_channels = 8;
	w->out.range = 10.0;
	w->out.position = 1;
	w->out.product_code = ETHERCAT_PID_EL4038;
	w->out.vendor_id = ETHERCAT_VID_BECKHOFF;
	w->out.sc = nullptr;
	w->out.offsets = nullptr;

	w->domain = nullptr;
	w->domain_pd = nullptr;
	w->domain_regs = nullptr;

	/* Placement new for C++ objects */
	new (&w->send) std::atomic<struct Sample *>();
	new (&w->thread) std::thread();
	new (&w->task) Task(CLOCK_REALTIME);

	return 0;
}

int villas::node::ethercat_destroy(NodeCompat *n)
{
	int ret;
	auto *w = n->getData<struct ethercat>();

	if (w->domain_regs)
		delete[] w->domain_regs;

	if (w->in.offsets)
		delete[] w->in.offsets;

	if (w->out.offsets)
		delete[] w->out.offsets;

	ret = queue_signalled_destroy(&w->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	w->task.~Task();

	/** @todo Destroy domain? */

	return 0;
}

int villas::node::ethercat_poll_fds(NodeCompat *n, int *fds)
{
	auto *w = n->getData<struct ethercat>();

	fds[0] = queue_signalled_fd(&w->queue);

	return 1;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "ethercat";
	p.description	= "Send and receive samples of an ethercat connection";
	p.vectorize	= 1; /* we only process a single sample per call */
	p.size		= sizeof(struct ethercat);
	p.type.start	= ethercat_type_start;
	p.type.stop	= ethercat_type_stop;
	p.parse		= ethercat_parse;
	p.print		= ethercat_print;
	p.check		= ethercat_check;
	p.init		= ethercat_init;
	p.destroy	= ethercat_destroy;
	p.prepare	= ethercat_prepare;
	p.start		= ethercat_start;
	p.stop		= ethercat_stop;
	p.read		= ethercat_read;
	p.write		= ethercat_write;
	p.poll_fds	= ethercat_poll_fds;

	static NodeCompatFactory ncp(&p);
}

