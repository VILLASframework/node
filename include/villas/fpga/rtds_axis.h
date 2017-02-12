/** Driver for AXI Stream wrapper around RTDS_InterfaceModule (rtds_axis )
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _FPGA_RTDS_AXIS_H_
#define _FPGA_RTDS_AXIS_H_

/* Forward declaration */
struct ip;

#define RTDS_HZ				100000000 // 100 MHz

#define RTDS_AXIS_MAX_TX		64	/**< The amount of values which is supported by the vfpga card */
#define RTDS_AXIS_MAX_RX		64	/**< The amount of values which is supported by the vfpga card */

/* Register offsets */
#define RTDS_AXIS_SR_OFFSET		0x00	/**< Status Register (read-only). See RTDS_AXIS_SR_* constant. */
#define RTDS_AXIS_CR_OFFSET		0x04	/**< Control Register (read/write) */
#define RTDS_AXIS_TSCNT_LOW_OFFSET	0x08	/**< Lower 32 bits of timestep counter (read-only). */
#define RTDS_AXIS_TSCNT_HIGH_OFFSET	0x0C	/**< Higher 32 bits of timestep counter (read-only). */
#define RTDS_AXIS_TS_PERIOD_OFFSET	0x10	/**< Period in clock cycles of previous timestep (read-only). */
#define RTDS_AXIS_COALESC_OFFSET	0x14	/**< IRQ Coalescing register (read/write). */
#define RTDS_AXIS_VERSION_OFFSET	0x18	/**< 16 bit version field passed back to the rack for version reporting (visible from “status” command, read/write). */
#define RTDS_AXIS_MRATE			0x1C	/**< Multi-rate register */

/* Status register bits */
#define RTDS_AXIS_SR_CARDDETECTED	(1 << 0)/**< ‘1’ when RTDS software has detected and configured card. */
#define RTDS_AXIS_SR_LINKUP		(1 << 1)/**< ‘1’ when RTDS communication link has been negotiated. */
#define RTDS_AXIS_SR_TX_FULL		(1 << 2)/**< Tx buffer is full, writes that happen when UserTxFull=’1’ will be dropped (Throttling / buffering is performed by hardware). */
#define RTDS_AXIS_SR_TX_INPROGRESS	(1 << 3)/**< Indicates when data is being put on link. */
#define RTDS_AXIS_SR_CASE_RUNNING	(1 << 4)/**< There is currently a simulation running. */

/* Control register bits */
#define RTDS_AXIS_CR_DISABLE_LINK	0	/**< Disable SFP TX when set */

void rtds_axis_dump(struct ip *c);

double rtds_axis_dt(struct ip *c);

#endif /* _FPGA_RTDS_AXIS_H_ */