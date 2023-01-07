/** Compile time configuration
 *
 * This file contains some compiled-in settings.
 * This settings are not part of the configuration file.
 *
 * @file
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017-2022, Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

// PCIe BAR number of VILLASfpga registers
#define FPGA_PCI_BAR		0
#define FPGA_PCI_VID_XILINX	0x10ee
#define FPGA_PCI_PID_VFPGA	0x7022

/** AXI Bus frequency for all components
 * except RTDS AXI Stream bridge which runs at RTDS_HZ (100 Mhz) */
#define FPGA_AXI_HZ		125000000 // 125 MHz
