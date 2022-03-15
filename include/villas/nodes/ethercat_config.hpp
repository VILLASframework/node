/** Configuration for Ethercat Node-type
 *
 * @file
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

#pragma once

#include <ecrt.h>

#define ETHERCAT_VID_BECKHOFF 0x00000002

#define ETHERCAT_PID_EK1100 0x044c2c52
#define ETHERCAT_PID_EL2004 0x07d43052
#define ETHERCAT_PID_EL2032 0x07f03052
#define ETHERCAT_PID_EL3152 0x0c503052
#define ETHERCAT_PID_EL3102 0x0c1e3052
#define ETHERCAT_PID_EL4102 0x10063052

// CWD Bus
#define ETHERCAT_PID_EL4038 0x0fc63052
#define ETHERCAT_PID_EL3008 0x0bc03052
#define ETHERCAT_PID_FC1100 0x044c0c62


/** @todo Make PDO entry tables configurable */

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
