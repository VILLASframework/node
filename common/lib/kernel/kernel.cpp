/** Linux kernel related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <sys/utsname.h>

#include <villas/kernel/kernel.hpp>
#include <villas/exceptions.hpp>

using namespace villas::utils;

Version villas::kernel::getVersion()
{
	struct utsname uts;

	if (uname(&uts) < 0)
		throw SystemError("Failed to retrieve system identification");

	std::string rel = uts.release;

	/* Remove release part. E.g. 4.9.93-linuxkit-aufs */
	auto sep = rel.find('-');
	auto ver = rel.substr(0, sep - 1);

	return Version(ver);
}
