/** Static server configuration
 *
 * This file contains some compiled-in settings.
 * This settings are not part of the configuration file.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

/* Paths */
#define PLUGIN_PATH		PREFIX "/share/villas/node/plugins"
#define WEB_PATH		PREFIX "/share/villas/node/web"
#define SYSFS_PATH		"/sys"
#define PROCFS_PATH		"/proc"

/** Default number of values in a sample */
#define DEFAULT_SAMPLELEN	64
#define DEFAULT_QUEUELEN	1024

/** Number of hugepages which are requested from the the kernel.
 * @see https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt */
#define DEFAULT_NR_HUGEPAGES	100

/** Width of log output in characters */
#define LOG_WIDTH		80
#define LOG_HEIGHT		25

/** Socket priority */
#define SOCKET_PRIO		7

/* Protocol numbers */
#define IPPROTO_VILLAS		137
#define ETH_P_VILLAS		0xBABE

#define USER_AGENT		"VILLASnode (" BUILDID ")"

/* Required kernel version */
#define KERNEL_VERSION_MAJ	3
#define KERNEL_VERSION_MIN	6

/** PCIe BAR number of VILLASfpga registers */
#define FPGA_PCI_BAR		0
#define FPGA_PCI_VID_XILINX	0x10ee
#define FPGA_PCI_PID_VFPGA	0x7022

/** AXI Bus frequency for all components
 * except RTDS AXI Stream bridge which runs at RTDS_HZ (100 Mhz) */
#define FPGA_AXI_HZ		125000000 // 125 MHz