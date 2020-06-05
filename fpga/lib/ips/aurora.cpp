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

#include <villas/utils.h>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/aurora.hpp>


/* Register offsets */
#define AUR_AXIS_SR_OFFSET			0x00	/**< Status Register (read-only) */
#define AUR_AXIS_CR_OFFSET			0x04	/**< Control Register (read/write) */
#define AUR_AXIS_CNTR_IN_H_OFFSET	0x0C	/**< Control Register (read/write) */
#define AUR_AXIS_CNTR_IN_L_OFFSET	0x08	/**< Control Register (read/write) */
#define AUR_AXIS_CNTR_OUT_H_OFFSET	0x18	/**< Control Register (read/write) */
#define AUR_AXIS_CNTR_OUT_L_OFFSET	0x1C	/**< Control Register (read/write) */

/* Status register bits */
#define AUR_AXIS_SR_CHAN_UP			(1 << 0)/**< 1-bit, asserted when channel initialisation is complete and is ready for data transfer */
#define AUR_AXIS_SR_LANE_UP			(1 << 1)/**< 1-bit, asserted for each lane upon successful lane initialisation */
#define AUR_AXIS_SR_HARD_ERR		(1 << 2)/**< 1-bit hard rror status */
#define AUR_AXIS_SR_SOFT_ERR		(1 << 3)/**< 1-bit soft error status */
#define AUR_AXIS_SR_FRAME_ERR		(1 << 4)/**< 1-bit frame error status */
#define AUR_AXIS_SR_HOT_PLUG		(1 << 5)/**< 1-bit, assserted when hot-plug count expires */

/* Control register bits */
#define AUR_AXIS_CR_LOOPBACK		(1 << 0)/**< 1-bit, assert to put Aurora IP in loopback mode. */
#define AUR_AXIS_CR_RST_CTRS		(1 << 1)/**< 1-bit, assert to reset counters */
#define AUR_AXIS_CR_SEQ_MODE		(1 << 2)/**< 2-bit, determines Sequence Number mode */

namespace villas {
namespace fpga {
namespace ip {

static AuroraFactory auroraFactoryInstance;


void Aurora::dump()
{
	/* Check Aurora AXI4 registers */
        const uint32_t sr = readMemory<uint32_t>(registerMemory, AUR_AXIS_SR_OFFSET);

	logger->info("Aurora-NovaCor AXI-Stream interface details:");
	logger->info("Aurora status:         {:#x}",  sr);
	logger->info("  Channel up:          {}", sr & AUR_AXIS_SR_CHAN_UP ? CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Lane up:             {}", sr & AUR_AXIS_SR_LANE_UP ? CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Hard error:          {}", sr & AUR_AXIS_SR_HARD_ERR ? CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Soft error:          {}", sr & AUR_AXIS_SR_SOFT_ERR ? CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Frame error:         {}", sr & AUR_AXIS_SR_FRAME_ERR ? CLR_GRN("yes") : CLR_RED("no"));
	logger->info("  Hot-plug count:      {}", sr & AUR_AXIS_SR_HOT_PLUG ? CLR_GRN("expired") : CLR_RED("not expired"));
}

AuroraFactory::AuroraFactory() :
    IpNodeFactory(getName())
{
}

} // namespace ip
} // namespace fpga
} // namespace villas
