/** Driver for wrapper around Aurora (acs.eonerc.rwth-aachen.de:user:aurora)
 *
 * @author Hatim Kanchwala <hatim@hatimak.me>
 * @copyright 2020, Hatim Kanchwala
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
#include <villas/fpga/ips/aurora.hpp>


/* Register offsets */
#define AURORA_AXIS_SR_OFFSET			0x00	/**< Status Register (read-only) */
#define AURORA_AXIS_CR_OFFSET			0x04	/**< Control Register (read/write) */
#define AURORA_AXIS_CNTR_IN_HIGH_OFFSET	0x0C	/**< Higher 32-bits of incoming frame counter */
#define AURORA_AXIS_CNTR_IN_LOW_OFFSET	0x08	/**< Lower 32-bits of incoming frame counter  */
#define AURORA_AXIS_CNTR_OUT_HIGH_OFFSET	0x18	/**< Higher 32-bits of outgoing frame counter  */
#define AURORA_AXIS_CNTR_OUT_LOW_OFFSET	0x1C	/**< Lower 32-bits of outgoing frame counter  */

/* Status register bits */
#define AURORA_AXIS_SR_CHAN_UP			(1 << 0)/**< 1-bit, asserted when channel initialisation is complete and is ready for data transfer */
#define AURORA_AXIS_SR_LANE_UP			(1 << 1)/**< 1-bit, asserted for each lane upon successful lane initialisation */
#define AURORA_AXIS_SR_HARD_ERR		(1 << 2)/**< 1-bit hard rror status */
#define AURORA_AXIS_SR_SOFT_ERR		(1 << 3)/**< 1-bit soft error status */
#define AURORA_AXIS_SR_FRAME_ERR		(1 << 4)/**< 1-bit frame error status */

/* Control register bits */
/** 1-bit, assert to put Aurora IP in loopback mode. */
#define AURORA_AXIS_CR_LOOPBACK		(1 << 0)

/** 1-bit, assert to reset counters, incoming and outgoing frame counters. */
#define AURORA_AXIS_CR_RST_CTRS		(1 << 1)

/** 1-bit, assert to turn off any sequence number handling by Aurora IP 
 * Sequence number must be handled in software then. */
#define AURORA_AXIS_CR_SEQ_MODE		(1 << 2)

/** 1-bit, assert to strip the received frame of the trailing sequence 
 * number. Sequence number mode must be set to handled by Aurora IP, 
 * otherwise this bit is ignored. */
#define AURORA_AXIS_CR_SEQ_STRIP		(1 << 3)

/** 1-bit, assert to use the same sequence number in the outgoing 
 * NovaCor-bound frames as the sequence number received from the 
 * incoming frames from NovaCor. Sequence number mode must be set to 
 * handled by Aurora IP, otherwise this bit is ignored.*/
#define AURORA_AXIS_CR_SEQ_ECHO		(1 << 4)


using namespace villas::fpga::ip;

static AuroraFactory auroraFactoryInstance;


void Aurora::dump()
{
	/* Check Aurora AXI4 registers */
	const uint32_t sr = readMemory<uint32_t>(registerMemory, AURORA_AXIS_SR_OFFSET);

	logger->info("Aurora-NovaCor AXI-Stream interface details:");
	logger->info("Aurora status:          {:#x}",  sr);
	logger->info("  Channel up:           {}", sr & AURORA_AXIS_SR_CHAN_UP ? CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Lane up:              {}", sr & AURORA_AXIS_SR_LANE_UP ? CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Hard error:           {}", sr & AURORA_AXIS_SR_HARD_ERR ? CLR_RED("yes") : CLR_GRN("no"));
	logger->info("  Soft error:           {}", sr & AURORA_AXIS_SR_SOFT_ERR ? CLR_RED("yes") : CLR_GRN("no"));
	logger->info("  Frame error:          {}", sr & AURORA_AXIS_SR_FRAME_ERR ? CLR_RED("yes") : CLR_GRN("no"));

	const uint64_t inCntLow  = readMemory<uint32_t>(registerMemory, AURORA_AXIS_CNTR_IN_LOW_OFFSET);
	const uint64_t inCntHigh = readMemory<uint32_t>(registerMemory, AURORA_AXIS_CNTR_IN_HIGH_OFFSET);
	const uint64_t inCnt = (inCntHigh << 32) | inCntLow;

	const uint64_t outCntLow  = readMemory<uint32_t>(registerMemory, AURORA_AXIS_CNTR_OUT_LOW_OFFSET);
	const uint64_t outCntHigh = readMemory<uint32_t>(registerMemory, AURORA_AXIS_CNTR_OUT_HIGH_OFFSET);
	const uint64_t outCnt = (outCntHigh << 32) | outCntLow;

	logger->info("Aurora frames received: {}", inCnt);
	logger->info("Aurora frames sent:     {}", outCnt);
}

void Aurora::setLoopback(bool state)
{
	auto cr = readMemory<uint32_t>(registerMemory, AURORA_AXIS_CR_OFFSET);

	if (state)
		cr |= AURORA_AXIS_CR_LOOPBACK;
	else
		cr &= ~AURORA_AXIS_CR_LOOPBACK;

	writeMemory<uint32_t>(registerMemory, AURORA_AXIS_CR_OFFSET, cr);
}

void Aurora::resetFrameCounters()
{
	auto cr = readMemory<uint32_t>(registerMemory, AURORA_AXIS_CR_OFFSET);

	cr |= AURORA_AXIS_CR_RST_CTRS;

	writeMemory<uint32_t>(registerMemory, AURORA_AXIS_CR_OFFSET, cr);
}
