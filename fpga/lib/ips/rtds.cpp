/** Driver for AXI Stream wrapper around RTDS_InterfaceModule (rtds_axis )
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Steffen Vogel
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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

#include <cstdint>

#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/rtds.hpp>


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

namespace villas {
namespace fpga {
namespace ip {

static RtdsFactory rtdsFactoryInstance;


void Rtds::dump()
{
	/* Check RTDS_Axis registers */
	const uint32_t sr = readMemory<uint32_t>(registerMemory, RTDS_AXIS_SR_OFFSET);

	logger->info("RTDS AXI Stream interface details:");
	logger->info("RTDS status:           {:#x}",  sr);
	logger->info("  Card detected:       {}", sr & RTDS_AXIS_SR_CARDDETECTED ?  CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Link up:             {}", sr & RTDS_AXIS_SR_LINKUP ?        CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  TX queue full:       {}", sr & RTDS_AXIS_SR_TX_FULL ?       CLR_RED("yes") : CLR_GRN("no"));
	logger->info("  TX in progress:      {}", sr & RTDS_AXIS_SR_TX_INPROGRESS ? CLR_YEL("yes") :         "no");
	logger->info("  Case running:        {}", sr & RTDS_AXIS_SR_CASE_RUNNING ?  CLR_GRN("yes") : CLR_RED("no"));

	logger->info("RTDS control:          {:#x}",	readMemory<uint32_t>(registerMemory, RTDS_AXIS_CR_OFFSET));
	logger->info("RTDS IRQ coalesc:      {}", readMemory<uint32_t>(registerMemory, RTDS_AXIS_COALESC_OFFSET));
	logger->info("RTDS IRQ version:      {:#x}", readMemory<uint32_t>(registerMemory, RTDS_AXIS_VERSION_OFFSET));
	logger->info("RTDS IRQ multi-rate:   {}", readMemory<uint32_t>(registerMemory, RTDS_AXIS_MRATE));

	const uint64_t timestepLow = readMemory<uint32_t>(registerMemory, RTDS_AXIS_TSCNT_LOW_OFFSET);
	const uint64_t timestepHigh = readMemory<uint32_t>(registerMemory, RTDS_AXIS_TSCNT_HIGH_OFFSET);
	const uint64_t timestep = (timestepHigh << 32) | timestepLow;

	logger->info("RTDS timestep counter: {}", timestep);
	logger->info("RTDS timestep period:  {:.3f} us", getDt() * 1e6);
}

double Rtds::getDt()
{
	const auto dt = readMemory<uint16_t>(registerMemory, RTDS_AXIS_TS_PERIOD_OFFSET);
	return (dt == 0xFFFF) ? 0.0 : (double) dt / RTDS_HZ;
}

RtdsFactory::RtdsFactory() :
    IpNodeFactory(getName(), getDescription())
{
}

} // namespace ip
} // namespace fpga
} // namespace villas
