/** Static server configuration
 *
 * This file contains some compiled-in settings.
 * This settings are not part of the configuration file.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#ifndef _GIT_REV
  #define _GIT_REV		"nogit"
#endif

/** The version number of VILLASnode */
#define VERSION			"v0.7-" _GIT_REV _VARIANT

/** Default number of values in a sample */
#define DEFAULT_VALUES		64
#define DEFAULT_QUEUELEN	1024

/** Number of hugepages which are requested from the the kernel.
 * @see https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt */
#define DEFAULT_NR_HUGEPAGES	25

/** Width of log output in characters */
#define LOG_WIDTH		132

/** Socket priority */
#define SOCKET_PRIO		7

/* Protocol numbers */
#define IPPROTO_VILLAS		137
#define ETH_P_VILLAS		0xBABE

#define SYSFS_PATH		"/sys"
#define PROCFS_PATH		"/proc"

#define USER_AGENT		"VILLASnode " VERSION

/* Required kernel version */
#define KERNEL_VERSION_MAJ	3
#define KERNEL_VERSION_MIN	6

/* Some hard-coded configuration for the FPGA benchmarks */
#define BENCH_DM		3
// 1 FIFO
// 2 DMA SG
// 3 DMA Simple

#define BENCH_RUNS		3000000
#define BENCH_WARMUP		100
#define BENCH_DM_EXP_MIN	0
#define BENCH_DM_EXP_MAX	20

/** PCIe BAR number of VILLASfpga registers */
#define FPGA_PCI_BAR		0
#define FPGA_PCI_VID_XILINX	0x10ee
#define FPGA_PCI_PID_VFPGA	0x7022

/** AXI Bus frequency for all components
 * except RTDS AXI Stream bridge which runs at RTDS_HZ (100 Mhz) */
#define FPGA_AXI_HZ		125000000 // 125 MHz