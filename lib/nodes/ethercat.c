//** Node type: Ethercat
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
#include <villas/nodes/ethercat.h>
#include <villas/format_type.h>
#include <villas/formats/msg_format.h>

/****************************************************************************/

#include "ecrt.h"

/****************************************************************************/

/** Task period in ns. */
#define PERIOD_NS   (1000000)

#define MAX_SAFE_STACK (8 * 1024) /* The maximum stack size which is
                                     guranteed safe to access without
                                     faulting */

/****************************************************************************/

/* Constants */
#define NSEC_PER_SEC (1000000000)
#define FREQUENCY (NSEC_PER_SEC / PERIOD_NS)

/****************************************************************************/

/* Forward declarations */
static struct plugin p;
static ec_master_t *master = NULL;
static ec_domain_t *domain1 = NULL;
static ec_slave_config_t *sc_ana_in = NULL;
static ec_slave_config_t *sc_ana_out = NULL;

// process data
static uint8_t *domain1_pd = NULL;

#define BusCouplerPos   0, 0
#define AnaOutSlavePos  0, 1
#define AnaInSlavePos   0, 2


#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL2004 0x00000002, 0x07d43052
#define Beckhoff_EL2032 0x00000002, 0x07f03052
#define Beckhoff_EL3152 0x00000002, 0x0c503052
#define Beckhoff_EL3102 0x00000002, 0x0c1e3052
#define Beckhoff_EL4102 0x00000002, 0x10063052

//CWD Bus
#define Beckhoff_EL4038 0x00000002, 0x0fc63052
#define Beckhoff_EL3008 0x00000002, 0x0bc03052
#define Beckhoff_FC1100 0x00000002, 0x044c0c62

// offsets for PDO entries
static unsigned int off_ana_out_values[8] = {0};
static unsigned int off_ana_in_values[8] = {0};

const static ec_pdo_entry_reg_t domain1_regs[] = {
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7000, 0x01, off_ana_out_values},
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7010, 0x01, off_ana_out_values+1},
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7020, 0x01, off_ana_out_values+2},
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7030, 0x01, off_ana_out_values+3},
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7040, 0x01, off_ana_out_values+4},
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7050, 0x01, off_ana_out_values+5},
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7060, 0x01, off_ana_out_values+6},
    {AnaOutSlavePos, Beckhoff_EL4038, 0x7070, 0x01, off_ana_out_values+7},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6000, 0x11, off_ana_in_values},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6010, 0x11, off_ana_in_values+1},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6020, 0x11, off_ana_in_values+2},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6030, 0x11, off_ana_in_values+3},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6040, 0x11, off_ana_in_values+4},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6050, 0x11, off_ana_in_values+5},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6060, 0x11, off_ana_in_values+6},
    {AnaInSlavePos,  Beckhoff_EL3008, 0x6070, 0x11, off_ana_in_values+7},
    {}
};

static unsigned int counter = 0;
//static unsigned int blink = 0;

/*****************************************************************************/

/* Master 0, Slave 3, "EL4038"
 * Vendor ID:       0x00000002
 * Product code:    0x0fc63052
 * Revision number: 0x00140000
 */

static ec_pdo_entry_info_t slave_3_pdo_entries[] = {
    {0x7000, 0x01, 16}, /* Analog output */
    {0x7010, 0x01, 16}, /* Analog output */
    {0x7020, 0x01, 16}, /* Analog output */
    {0x7030, 0x01, 16}, /* Analog output */
    {0x7040, 0x01, 16}, /* Analog output */
    {0x7050, 0x01, 16}, /* Analog output */
    {0x7060, 0x01, 16}, /* Analog output */
    {0x7070, 0x01, 16}, /* Analog output */
};

static ec_pdo_info_t slave_3_pdos[] = {
    {0x1600, 1, slave_3_pdo_entries + 0}, /* AO RxPDO-Map Outputs Ch.1 */
    {0x1601, 1, slave_3_pdo_entries + 1}, /* AO RxPDO-Map Outputs Ch.2 */
    {0x1602, 1, slave_3_pdo_entries + 2}, /* AO RxPDO-Map Outputs Ch.3 */
    {0x1603, 1, slave_3_pdo_entries + 3}, /* AO RxPDO-Map Outputs Ch.4 */
    {0x1604, 1, slave_3_pdo_entries + 4}, /* AO RxPDO-Map Outputs Ch.5 */
    {0x1605, 1, slave_3_pdo_entries + 5}, /* AO RxPDO-Map Outputs Ch.6 */
    {0x1606, 1, slave_3_pdo_entries + 6}, /* AO RxPDO-Map Outputs Ch.7 */
    {0x1607, 1, slave_3_pdo_entries + 7}, /* AO RxPDO-Map Outputs Ch.8 */
};

static ec_sync_info_t slave_3_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 8, slave_3_pdos + 0, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {0xff}
};

/* Master 0, Slave 4, "EL3008"
 * Vendor ID:       0x00000002
 * Product code:    0x0bc03052
 * Revision number: 0x00150000
 */

static ec_pdo_entry_info_t slave_4_pdo_entries[] = {
    {0x6000, 0x01, 1}, /* Underrange */
    {0x6000, 0x02, 1}, /* Overrange */
    {0x6000, 0x03, 2}, /* Limit 1 */
    {0x6000, 0x05, 2}, /* Limit 2 */
    {0x6000, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6000, 0x0f, 1}, /* TxPDO State */
    {0x6000, 0x10, 1}, /* TxPDO Toggle */
    {0x6000, 0x11, 16}, /* Value */
    {0x6010, 0x01, 1}, /* Underrange */
    {0x6010, 0x02, 1}, /* Overrange */
    {0x6010, 0x03, 2}, /* Limit 1 */
    {0x6010, 0x05, 2}, /* Limit 2 */
    {0x6010, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6010, 0x0f, 1}, /* TxPDO State */
    {0x6010, 0x10, 1}, /* TxPDO Toggle */
    {0x6010, 0x11, 16}, /* Value */
    {0x6020, 0x01, 1}, /* Underrange */
    {0x6020, 0x02, 1}, /* Overrange */
    {0x6020, 0x03, 2}, /* Limit 1 */
    {0x6020, 0x05, 2}, /* Limit 2 */
    {0x6020, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6020, 0x0f, 1}, /* TxPDO State */
    {0x6020, 0x10, 1}, /* TxPDO Toggle */
    {0x6020, 0x11, 16}, /* Value */
    {0x6030, 0x01, 1}, /* Underrange */
    {0x6030, 0x02, 1}, /* Overrange */
    {0x6030, 0x03, 2}, /* Limit 1 */
    {0x6030, 0x05, 2}, /* Limit 2 */
    {0x6030, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6030, 0x0f, 1}, /* TxPDO State */
    {0x6030, 0x10, 1}, /* TxPDO Toggle */
    {0x6030, 0x11, 16}, /* Value */
    {0x6040, 0x01, 1}, /* Underrange */
    {0x6040, 0x02, 1}, /* Overrange */
    {0x6040, 0x03, 2}, /* Limit 1 */
    {0x6040, 0x05, 2}, /* Limit 2 */
    {0x6040, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6040, 0x0f, 1}, /* TxPDO State */
    {0x6040, 0x10, 1}, /* TxPDO Toggle */
    {0x6040, 0x11, 16}, /* Value */
    {0x6050, 0x01, 1}, /* Underrange */
    {0x6050, 0x02, 1}, /* Overrange */
    {0x6050, 0x03, 2}, /* Limit 1 */
    {0x6050, 0x05, 2}, /* Limit 2 */
    {0x6050, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6050, 0x0f, 1}, /* TxPDO State */
    {0x6050, 0x10, 1}, /* TxPDO Toggle */
    {0x6050, 0x11, 16}, /* Value */
    {0x6060, 0x01, 1}, /* Underrange */
    {0x6060, 0x02, 1}, /* Overrange */
    {0x6060, 0x03, 2}, /* Limit 1 */
    {0x6060, 0x05, 2}, /* Limit 2 */
    {0x6060, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6060, 0x0f, 1}, /* TxPDO State */
    {0x6060, 0x10, 1}, /* TxPDO Toggle */
    {0x6060, 0x11, 16}, /* Value */
    {0x6070, 0x01, 1}, /* Underrange */
    {0x6070, 0x02, 1}, /* Overrange */
    {0x6070, 0x03, 2}, /* Limit 1 */
    {0x6070, 0x05, 2}, /* Limit 2 */
    {0x6070, 0x07, 1}, /* Error */
    {0x0000, 0x00, 1}, /* Gap */
    {0x0000, 0x00, 6}, /* Gap */
    {0x6070, 0x0f, 1}, /* TxPDO State */
    {0x6070, 0x10, 1}, /* TxPDO Toggle */
    {0x6070, 0x11, 16}, /* Value */
};

static ec_pdo_info_t slave_4_pdos[] = {
    {0x1a00, 10, slave_4_pdo_entries + 0}, /* AI TxPDO-Map Standard Ch.1 */
    {0x1a02, 10, slave_4_pdo_entries + 10}, /* AI TxPDO-Map Standard Ch.2 */
    {0x1a04, 10, slave_4_pdo_entries + 20}, /* AI TxPDO-Map Standard Ch.3 */
    {0x1a06, 10, slave_4_pdo_entries + 30}, /* AI TxPDO-Map Standard Ch.4 */
    {0x1a08, 10, slave_4_pdo_entries + 40}, /* AI TxPDO-Map Standard Ch.5 */
    {0x1a0a, 10, slave_4_pdo_entries + 50}, /* AI TxPDO-Map Standard Ch.6 */
    {0x1a0c, 10, slave_4_pdo_entries + 60}, /* AI TxPDO-Map Standard Ch.7 */
    {0x1a0e, 10, slave_4_pdo_entries + 70}, /* AI TxPDO-Map Standard Ch.8 */
};

static ec_sync_info_t slave_4_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 8, slave_4_pdos + 0, EC_WD_DISABLE},
    {0xff}
};


int ethercat_type_start(struct super_node *sn)
{
	ec_slave_config_t *sc;
	struct timespec_wakeup_time;
	int ret = 0;

	void stack_prefault(void)
{
    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
}

    master = ecrt_request_master(0);
	if(!master) {
		return -1;
	}

	domain1 = ecrt_master_create_domain(master);
	if(!domain1){
		return -1;
	}

	//Create configuration for bus coupler
    sc = ecrt_master_slave_config(master, BusCouplerPos< Beckhoff_EK1100);
	if(!sc){
		return -1;
	}

	return 0;
}

int ethercat_start(struct node *n)
{
	int ret;
	struct ethercat *w = (struct ethercat *) n->_vd;

	ret = pool_init(&w->pool, DEFAULT_ETHERCAT_QUEUE_LENGTH, SAMPLE_LENGTH(DEFAULT_ETHERCAT_SAMPLE_LENGTH), &memory_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_ETHERCAT_QUEUE_LENGTH, &memory_hugepage, 0);
	if (ret)
		return ret;

       //Configure analog in
    printf("Configuring PDOs...\n");
    if (!(sc_ana_in = ecrt_master_slave_config(
                    master, AnaInSlavePos, Beckhoff_EL3008))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    if (ecrt_slave_config_pdos(sc_ana_in, EC_END, slave_4_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

   //Configure analog out
    if (!(sc_ana_out = ecrt_master_slave_config(
                    master, AnaOutSlavePos, Beckhoff_EL4038))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    if (ecrt_slave_config_pdos(sc_ana_out, EC_END, slave_3_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

	    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return -1;
    }

    printf("Activating master...\n");
    if (ecrt_master_activate(master)) {
        return -1;
    }

    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return -1;
    }



	return 0;
}

int ethercat_stop(struct node *n)
{
	int ret;
	struct ethercat *w = (struct ethercat *) n->_vd;
	printf("releasing master"\n);
    ecrt_release_master(master);
	printf("have a nice day!\n")

	ret = queue_signalled_destroy(&w->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	return 0;
}

int ethercat_destroy(struct node *n)
{
	struct websocket *w = (struct websocket *) n->_vd;
	int ret;

    /* TODO: destroy ethercat connection */

	if (ret)
		return ret;

	return 0;
}

int ethercat_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct ethercat *w = (struct ethercat *) n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
	if (avail < 0)
		return avail;

	  // receive process data
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);

#if 1
        //read process data
        for(int i=0; i<8; ++i) {
			int16_t ain_value + EC_READ_S16(domain1_pd + Off_ana_in_values[i]);
			
			float ain_voltage = 10.0 * (float) ain_value/INT16_MAX;
            printf("AnaIn(%d): value=%f Volts\n", i, ain_voltage);
        }
        printf("\n");
#endif


	sample_copy_many(smps, cpys, avail);
	sample_decref_many(cpys, avail);

	return avail;
}

int ethercat_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct ethercat *w = (struct ethercat *) n->_vd;
	struct sample *cpys[cnt];

	/* Make copies of all samples */
	avail = sample_alloc_many(&w->pool, cpys, cnt);
	if (avail < cnt)
		warn("Pool underrun for node %s: avail=%u", node_name(n), avail);

	sample_copy_many(cpys, smps, avail);

   
#if 1
    // write process data
    for(int i=0; i<8; ++i) {
		float aout_voltage = i * 1.0;

		in16_t aout_value = aout_voltage / 10.0 * INT16_MAX;

		if (counter == 0)
			printf("AnaOut(%d) : value=%f Volts\n", i, aout_voltage);

        EC_WRITE_U16(domain1_pd + off_ana_out_values[i], 0x8000);
    }
    //EC_WRITE_U8(domain1_pd + off_dig_out, blink ? 0x06 : 0x09);
#endif

    // send process data
    //printf("queue\n");
    ecrt_domain_queue(domain1);
    //printf("send\n");
    ecrt_master_send(master);
}



	sample_decref_many(cpys, avail);

	return cnt;
}

int ethercat_parse(struct node *n, json_t *cfg)
{
	struct ethercat *w = (struct ethercat *) n->_vd;
	int ret;

	size_t i;
	json_t *json_dests = NULL;
	json_t *json_dest;
	json_error_t err;

    /* TODO: parse json */

	return 0;
}

char * ethercat_print(struct node *n)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	char *buf = NULL;
    /* TODO: maybe use asprintf to build string? */

	return buf;
}

int ethercat_fd(struct node *n)
{
	struct ethercat *w = (struct ethercat *) n->_vd;

	return queue_signalled_fd(&w->queue);
}

static struct plugin p = {
	.name		= "ethercat",
	.description	= "Send and receive samples of an ethercat connection",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct ethercat),
		.type.start	= ethercat_type_start,
		.start		= ethercat_start,
		.stop		= ethercat_stop,
		.destroy	= ethercat_destroy,
		.read		= ethercat_read,
		.write		= ethercat_write,
		.print		= ethercat_print,
		.parse		= ethercat_parse,
		.fd		    = ethercat_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
