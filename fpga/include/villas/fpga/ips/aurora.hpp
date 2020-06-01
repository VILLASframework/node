/** Driver for wrapper around Aurora (acs.eonerc.rwth-aachen.de:user:aurora)
 *
 * @file
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <villas/fpga/ip_node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Aurora : public IpNode {
public:
	static constexpr const char* masterPort = "m_axis";
	static constexpr const char* slavePort = "s_axis";

	void dump();

	std::list<std::string> getMemoryBlocks() const
	{ return { registerMemory }; }

	const StreamVertex&
	getDefaultSlavePort() const
	{ return getSlavePort(slavePort); }

	const StreamVertex&
	getDefaultMasterPort() const
	{ return getMasterPort(masterPort); }

private:
	static constexpr const char registerMemory[] = "reg0";
};


class AuroraFactory : public IpNodeFactory {
public:
	AuroraFactory();

	IpCore* create()
	{ return new Aurora; }

	std::string
	getName() const
	{ return "Aurora"; }

	std::string
	getDescription() const
	{ return "Aurora 8B/10B and additional support modules, like, communication with NovaCor and read/write status/control registers."; }

	Vlnv getCompatibleVlnv() const
	{ return {"acs.eonerc.rwth-aachen.de:user:aurora:"}; }
};

} // namespace ip
} // namespace fpga
} // namespace villas

/** @} */
