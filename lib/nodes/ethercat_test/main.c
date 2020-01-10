/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2007-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 ****************************************************************************/

#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h> /* clock_gettime() */
#include <sys/mman.h> /* mlockall() */

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

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

static ec_slave_config_t *sc_ana_in = NULL;
static ec_slave_config_state_t sc_ana_in_state = {};

static ec_slave_config_t *sc_ana_out = NULL;
static ec_slave_config_state_t sc_ana_out_state = {};

/****************************************************************************/

// Process data
static uint8_t *domain1_pd = NULL;

#if 0
#define MediaConvACSPos 0, 0
#define MediaConvCWDPos 0, 1
#define BusCouplerPos   0, 2
#define AnaOutSlavePos  0, 3
#define AnaInSlavePos   0, 4
#define PCISlavePos     0, 5
#else
#define BusCouplerPos   0, 0
#define AnaOutSlavePos  0, 1
#define AnaInSlavePos   0, 2
#endif

#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL2004 0x00000002, 0x07d43052
#define Beckhoff_EL2032 0x00000002, 0x07f03052
#define Beckhoff_EL3152 0x00000002, 0x0c503052
#define Beckhoff_EL3102 0x00000002, 0x0c1e3052
#define Beckhoff_EL4102 0x00000002, 0x10063052

// CWD Bus
#define Beckhoff_EL4038 0x00000002, 0x0fc63052
#define Beckhoff_EL3008 0x00000002, 0x0bc03052
#define Beckhoff_FC1100 0x00000002, 0x044c0c62

// Offsets for PDO entries
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

/*****************************************************************************/

static const char* DOMAIN_STATES[] = {[0]="ZERO",
                                      [1]="INCOMPLETE",
                                      [2]="COMPLETE",
                                     };
void check_domain1_state(void)
{
    ec_domain_state_t ds;

    ecrt_domain_state(domain1, &ds);

    /*if (ds.working_counter != domain1_state.working_counter) {
        printf("Domain1: new working counter %u.\n", ds.working_counter);
    }*/
    if (ds.wc_state != domain1_state.wc_state) {
        printf("Domain1: State %s.\n", DOMAIN_STATES[ds.wc_state]);
    }

    domain1_state = ds;
}

/*****************************************************************************/

static const char* MASTER_AL_STATES[] = {[1]="INIT",
                                         [2]="PREOP",
                                         [4]="SAFEOP",
                                         [8]="OP"};

void check_master_state(void)
{
    ec_master_state_t ms;

    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding) {
        printf("%u slave(s).\n", ms.slaves_responding);
    }
    if (ms.al_states != master_state.al_states) {
        if (ms.al_states >= (1<<4)) {
            printf("AL states: unknown: 0x%02X\n", ms.al_states);
        } else {
            printf("AL states: ");
            for (unsigned char i = 1; i < (1<<4); i <<= 1) {
                if (ms.al_states & i) {
                    printf("%s, ", MASTER_AL_STATES[i]);
                }
            }
            printf("\n");
        }
    }
    if (ms.link_up != master_state.link_up) {
        printf("Link is %s.\n", ms.link_up ? "up" : "down");
    }

    master_state = ms;
}

/*****************************************************************************/

static const char* SLAVE_AL_STATES[] = {[1]="INIT",
                                        [2]="PREOP",
                                        [4]="SAFEOP",
                                        [8]="OP"};

void check_slave_config_states(void)
{
    ec_slave_config_state_t s;

    ecrt_slave_config_state(sc_ana_in, &s);

    if (s.al_state != sc_ana_in_state.al_state) {
        if (s.al_state > 8) {
            printf("AnaIn state: unknown: 0x%02X\n", s.al_state);
        } else {
            printf("AnaIn state: %s.\n", SLAVE_AL_STATES[s.al_state]);
        }
    }
    if (s.online != sc_ana_in_state.online) {
        printf("AnaIn: %s.\n", s.online ? "online" : "offline");
    }
    if (s.operational != sc_ana_in_state.operational) {
        printf("AnaIn: %soperational.\n", s.operational ? "" : "Not ");
    }

    sc_ana_in_state = s;


    ecrt_slave_config_state(sc_ana_out, &s);

    if (s.al_state != sc_ana_out_state.al_state) {
        if (s.al_state > 8) {
            printf("AnaOut state: unknown: 0x%02X\n", s.al_state);
        } else {
            printf("AnaOut state: %s.\n", SLAVE_AL_STATES[s.al_state]);
        }
    }
    if (s.online != sc_ana_out_state.online) {
        printf("AnaOut: %s.\n", s.online ? "online" : "offline");
    }
    if (s.operational != sc_ana_out_state.operational) {
        printf("AnaOut: %soperational.\n", s.operational ? "" : "Not ");
    }

    sc_ana_out_state = s;
}

/*****************************************************************************/

void cyclic_task()
{
    // receive process data
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);

    // check process data state
    check_domain1_state();

    if (counter) {
        counter--;
    } else { // do this at 1 Hz
        counter = FREQUENCY;

#if 1
        // Read process data
        for(int i=0; i<8; ++i) {
	    int16_t ain_value = EC_READ_S16(domain1_pd + off_ana_in_values[i]);

            float ain_voltage = 10.0 * (float) ain_value / INT16_MAX;

            printf("AnaIn(%d): value=%f Volts\n", i, ain_voltage);
        }
        printf("\n");
#endif
        // check for master state (optional)
        check_master_state();

        // check for slave configuration state(s) (optional)
        check_slave_config_states();
    }


#if 1
    // write process data
    for (int i=0; i<8; ++i) {
	float aout_voltage = i * 1.0;

	int16_t aout_value = aout_voltage / 10.0 * INT16_MAX;

	if (counter == 0)
		printf("AnaOut(%d): value=%f Volts\n", i, aout_voltage);

        EC_WRITE_U16(domain1_pd + off_ana_out_values[i], aout_value);
    }
    //EC_WRITE_U8(domain1_pd + off_dig_out, blink ? 0x06 : 0x09);
#endif

    // send process data
    //printf("queue\n");
    ecrt_domain_queue(domain1);
    //printf("send\n");
    ecrt_master_send(master);
}

/****************************************************************************/

void stack_prefault(void)
{
    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
}

/****************************************************************************/

int main(int argc, char **argv)
{
    ec_slave_config_t *sc;
    struct timespec wakeup_time;
    int ret = 0;

    master = ecrt_request_master(0);
    if (!master) {
        return -1;
    }

    domain1 = ecrt_master_create_domain(master);
    if (!domain1) {
        return -1;
    }

    // Create configuration for bus coupler
    sc = ecrt_master_slave_config(master, BusCouplerPos, Beckhoff_EK1100);
    if (!sc) {
        return -1;
    }

    // Configure analog in
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

    // Configure analog out
    if (!(sc_ana_out = ecrt_master_slave_config(
                    master, AnaOutSlavePos, Beckhoff_EL4038))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    if (ecrt_slave_config_pdos(sc_ana_out, EC_END, slave_3_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

    // Configure PCI Slave
    /*sc = ecrt_master_slave_config(master, PCISlavePos, Beckhoff_FC1100);
    if (!sc) {
        fprintf(stderr, "failed to configure PCI Card (FC1100)\n");
        return -1;
    }*/


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

    /* Set priority */

    pid_t pid = getpid();
    if (setpriority(PRIO_PROCESS, pid, -19)) {
        fprintf(stderr, "Warning: Failed to set priority: %s\n",
                strerror(errno));
    }

    /* Lock memory */

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        fprintf(stderr, "Warning: Failed to lock memory: %s\n",
                strerror(errno));
    }

    stack_prefault();

    printf("Starting RT task with dt=%u ns.\n", PERIOD_NS);

    clock_gettime(CLOCK_MONOTONIC, &wakeup_time);
    wakeup_time.tv_sec += 1; /* start in future */
    wakeup_time.tv_nsec = 0;

    for (int i=0; i != 10000; ++i) {
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                &wakeup_time, NULL);
        if (ret) {
            fprintf(stderr, "clock_nanosleep(): %s\n", strerror(ret));
            return -1;//break;
        }

        cyclic_task();

        wakeup_time.tv_nsec += PERIOD_NS;

        while (wakeup_time.tv_nsec >= NSEC_PER_SEC) {
            wakeup_time.tv_nsec -= NSEC_PER_SEC;
            wakeup_time.tv_sec++;
        }
    }
    printf("releasing master\n");
    ecrt_release_master(master);
    printf("have a nice day!\n");

    return ret;
}

/****************************************************************************/
