/** Helper function for directly using VILLASfpga outside of VILLASnode
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @copyright 2022, Steffen Vogel, Niklas Eiling
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

#pragma once

#include <string>
#include <villas/fpga/card.hpp>

namespace villas {
namespace fpga {

std::shared_ptr<fpga::PCIeCard>
setupFpgaCard(const std::string &configFile, const std::string &fpgaName);

void configCrossBarUsingConnectString(std::string connectString,
	std::shared_ptr<villas::fpga::ip::Dma> dma,
	std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>>& aurora_channels);

void setupColorHandling();

} /* namespace fpga */
} /* namespace villas */
