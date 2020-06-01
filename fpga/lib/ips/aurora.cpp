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
#define AUR_AXIS_SR_OFFSET		0x00	/**< Status Register (read-only). See AUR_AXIS_SR_* constant. */
#define AUR_AXIS_CR_OFFSET		0x04	/**< Control Register (read/write) */

/* Status register bits */
#define AUR_AXIS_SR_LOOPBACK		(1 << 0)/**< ‘1’ when Aurora IP is in loopback mode. */

/* Control register bits */

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
	logger->info("  Loopback mode:       {}", sr & AUR_AXIS_SR_LOOPBACK ? CLR_GRN("yes") : CLR_RED("no"));
}

AuroraFactory::AuroraFactory() :
    IpNodeFactory(getName())
{
}

} // namespace ip
} // namespace fpga
} // namespace villas
