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
#include <villas/fpga/pcie_card.hpp>

namespace villas {
namespace fpga {

std::shared_ptr<fpga::PCIeCard>
setupFpgaCard(const std::string &configFile, const std::string &fpgaName);

void setupColorHandling();

class ConnectString {
public:
	ConnectString(std::string& connectString, int maxPortNum = 7);

	void parseString(std::string& connectString);
	int portStringToInt(std::string &str) const;

	void configCrossBar(std::shared_ptr<villas::fpga::ip::Dma> dma,
		std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>>& aurora_channels) const;

	bool isBidirectional() const { return bidirectional; };
	bool isDmaLoopback() const { return dmaLoopback; };
	bool isSrcStdin() const { return srcIsStdin; };
	bool isDstStdout() const { return dstIsStdout; };
	int getSrcAsInt() const { return srcAsInt; };
	int getDstAsInt() const { return dstAsInt; };
protected:
	villas::Logger log;
	int maxPortNum;
	bool bidirectional;
	bool invert;
	int srcAsInt;
	int dstAsInt;
	bool srcIsStdin;
	bool dstIsStdout;
	bool dmaLoopback;
};


} /* namespace fpga */
} /* namespace villas */
