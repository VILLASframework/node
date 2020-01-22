/** Node type: Ethercat
 *
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <villas/super_node.h>
#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/buffer.h>
#include <villas/plugin.h>
#include <villas/log.h>
#include <villas/format_type.h>
#include <villas/formats/msg_format.h>

#include <villas/nodes/ethercat.h>

/** TODOs
 *
 * - Implement ethercat_parse()
 *

  "{ s: i, s: { s: i }, s: { } }",
	"alias", &w->alias,
  	"in",
	  	"num_channels", &w->in.num_channels,
		"pid", ...
		"vid", ...
	"out",
 
	  - alias
	  - coupler_pos

	  - in.pos
	  - in.num_channels
	  - in.voltage_range
	  - in.vid
	  - in.pid

	  - out...

	  - coupler_vid
	  - coupler_pid

 * - Implement ethercat_print()
 * - Implement ethercat_destroy()
 * - Replace printf() by calls to info(), warning(), error()
 */


/****************************************************************************/

/** Task period in ns. */
#define PERIOD_NS   (1000000)

/****************************************************************************/

/* Constants */
#define NSEC_PER_SEC (1000000000)
#define FREQUENCY (NSEC_PER_SEC / PERIOD_NS)

/****************************************************************************/

/* Forward declarations */
static struct plugin p;

static ec_master_t *master = NULL;

int ethercat_type_start(struct super_node *sn)
{
	ec_slave_config_t *sc;
	struct timespec_wakeup_time;

	master = ecrt_request_master(0);
	if (!master) {
		return -1;
	}

	// Create configuration for bus coupler
	sc = ecrt_master_slave_config(master, ETHERCAT_ALIAS, ETHERCAT_POS_COUPLER, ETHERCAT_VID_BECKHOFF, ETHERCAT_PID_EK1100);
	if (!sc) {
		return -1;
	}

	return 0;
}

int ethercat_start(struct node *n)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	// int ret = pool_init(&w->pool, DEFAULT_ETHERCAT_QUEUE_LENGTH, SAMPLE_LENGTH(DEFAULT_ETHERCAT_SAMPLE_LENGTH), &memory_hugepage);
	// if (ret)
	// 	return ret;

	// ret = queue_signalled_init(&w->queue, DEFAULT_ETHERCAT_QUEUE_LENGTH, &memory_hugepage, 0);
	// if (ret)
	// 	return ret;

	w->domain = ecrt_master_create_domain(master);
	if (!w->domain) {
		return -1;
	}

	// Prepare list of domain registers
	for (int i = 0; i < ETHERCAT_NUM_CHANNELS; i++)
	{
		w->off_out_values[i] = 0;
		w->off_in_values[i] = 0;

		w->domain_regs[i].alias = ETHERCAT_ALIAS;
		w->domain_regs[i].position = ETHERCAT_POS_SLAVE_OUT;
		w->domain_regs[i].vendor_id = ETHERCAT_VID_BECKHOFF;
		w->domain_regs[i].product_code = ETHERCAT_PID_EL4038;
		w->domain_regs[i].index = 0x7000 + i * 0x10;
		w->domain_regs[i].subindex = 0x1;
		w->domain_regs[i].offset = w->off_out_values + i;

		w->domain_regs[i+ETHERCAT_NUM_CHANNELS].alias = ETHERCAT_ALIAS;
		w->domain_regs[i+ETHERCAT_NUM_CHANNELS].position = ETHERCAT_POS_SLAVE_IN;
		w->domain_regs[i+ETHERCAT_NUM_CHANNELS].vendor_id = ETHERCAT_VID_BECKHOFF;
		w->domain_regs[i+ETHERCAT_NUM_CHANNELS].product_code = ETHERCAT_PID_EL3008;
		w->domain_regs[i+ETHERCAT_NUM_CHANNELS].index = 0x6000 + i * 0x10;
		w->domain_regs[i+ETHERCAT_NUM_CHANNELS].subindex = 0x11;
		w->domain_regs[i+ETHERCAT_NUM_CHANNELS].offset = w->off_in_values + i;
	};

	// End of list delimiter
	memset(&w->domain_regs[2*ETHERCAT_NUM_CHANNELS], 0, sizeof(ec_pdo_entry_reg_t));

	// Configure analog in
	info("Configuring PDOs...\n");
	if (!(w->sc_in = ecrt_master_slave_config(master, ETHERCAT_ALIAS, ETHERCAT_POS_SLAVE_IN, ETHERCAT_VID_BECKHOFF, ETHERCAT_PID_EL3008))) {
		warning("Failed to get slave configuration.\n");
		return -1;
	}

	if (ecrt_slave_config_pdos(w->sc_in, EC_END, slave_4_syncs)) {
		error("Failed to configure PDOs.\n");
		return -1;
	}

	// Configure analog out
	if (!(w->sc_out = ecrt_master_slave_config(master, ETHERCAT_ALIAS, ETHERCAT_POS_SLAVE_OUT, ETHERCAT_VID_BECKHOFF, ETHERCAT_PID_EL4038))) {
		warning("Failed to get slave configuration.\n");
		return -1;
	}

	if (ecrt_slave_config_pdos(w->sc_out, EC_END, slave_3_syncs)) {
		warning("Failed to configure PDOs.\n");
		return -1;
	}

		if (ecrt_domain_reg_pdo_entry_list(w->domain, w->domain_regs)) {
		error("PDO entry registration failed!\n");
		return -1;
	}

	info("Activating master...\n");
	if (ecrt_master_activate(master)) {
		return -1;
	}

	if (!(w->domain_pd = ecrt_domain_data(w->domain))) {
		return -1;
	}

	return 0;
}

int ethercat_stop(struct node *n)
{
	int ret;
	struct ethercat *w = (struct ethercat *) n->_vd;
	info("releasing master\n");
	ecrt_release_master(master);
	info("have a nice day!\n");

	ret = queue_signalled_destroy(&w->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	return 0;
}

int ethercat_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	// int avail;
	// struct sample *cpys[cnt];

	// avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
	// if (avail < 0)
	// 	return avail;

	// Receive process data
	ecrt_master_receive(master);
	ecrt_domain_process(w->domain);

	if (cnt < 1)
		return 0;

	// Read process data
	for (int i = 0; i < MIN(ETHERCAT_NUM_CHANNELS, smps[0]->capacity); ++i) {
		int16_t ain_value = EC_READ_S16(w->domain_pd + w->off_in_values[i]);
			
		smps[0]->data[i].f = ETHERCAT_VOLTAGE_RANGE * (float) ain_value / INT16_MAX; 
	}

	// sample_copy_many(smps, cpys, avail);
	// sample_decref_many(cpys, avail);

	return 1;
}

int ethercat_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	/* Make copies of all samples */
	// struct sample *cpys[cnt];
	// int avail = sample_alloc_many(&w->pool, cpys, cnt);
	// if (avail < cnt)
	// 	warning("Pool underrun for node %s: avail=%u", node_name(n), avail);

	// sample_copy_many(cpys, smps, avail);

	if (cnt < 1)
		return 0;

	// Write process data
	for (int i = 0; i < ETHERCAT_NUM_CHANNELS; ++i) {
		float aout_voltage = smps[0]->data[i].f;

		int16_t aout_value = aout_voltage / ETHERCAT_VOLTAGE_RANGE * INT16_MAX;

		EC_WRITE_U16(w->domain_pd + w->off_out_values[i], aout_value);
	}

	// send process data
	ecrt_domain_queue(w->domain);
	ecrt_master_send(master);

	//sample_decref_many(cpys, avail);

	return 1;
}

int ethercat_parse(struct node *n, json_t *cfg)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	//const char *local, *remote;
	//const char *layer = NULL;
	const char *format = "villas.binary";

	int ret;

	json_error_t err;
 

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: { s: i, s?: i, s?: i }, s?: { s: i, s?: i, s?: i } }",
		"alias", &alias,
		"out",
			"num_channels", &w->in.num_channels,
			"pid", &w->out.pid,
			"vid", &w->out.vid,
		"in",
			"num_channels", &w->in.num_channels,
			"pid", &w->in.pid,
			"vid", &w->in.vid,
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	/* Format */
	w->format = format_type_lookup(format);
	if (!w->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	return 0;
}


// int ethercat_poll_fds(struct node *n, int *fds)
// {
// 	struct ethercat *w = (struct ethercat *) n->_vd;

// 	fds[0] = queue_signalled_fd(&w->queue);

// 	return 1;
// }

static struct plugin p = {
	.name		= "ethercat",
	.description	= "Send and receive samples of an ethercat connection",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 1, /* we only process a single sample per call */
		.size		= sizeof(struct ethercat),
		.type.start	= ethercat_type_start,
		.start		= ethercat_start,
		.parse		= ethercat_parse,
		.stop		= ethercat_stop,
		.read		= ethercat_read,
		.write		= ethercat_write,
		// .poll_fds   = ethercat_poll_fds
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
