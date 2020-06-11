/** Vendor, Library, Name, Version (VLNV) tag
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <string>
#include <sstream>

#include <villas/fpga/vlnv.hpp>

namespace villas {
namespace fpga {

bool
Vlnv::operator==(const Vlnv &other) const
{
	// if a field is empty, it means wildcard matching everything
	const bool vendorWildcard	= vendor.empty()	or other.vendor.empty();
	const bool libraryWildcard	= library.empty()	or other.library.empty();
	const bool nameWildcard		= name.empty()		or other.name.empty();
	const bool versionWildcard	= version.empty()	or other.version.empty();

	const bool vendorMatch		= vendorWildcard	or vendor == other.vendor;
	const bool libraryMatch		= libraryWildcard	or library == other.library;
	const bool nameMatch		= nameWildcard		or name == other.name;
	const bool versionMatch		= versionWildcard	or version == other.version;

	return vendorMatch and libraryMatch and nameMatch and versionMatch;
}

void
Vlnv::parseFromString(std::string vlnv)
{
	// tokenize by delimiter
	std::stringstream sstream(vlnv);
	std::getline(sstream, vendor,	delimiter);
	std::getline(sstream, library,	delimiter);
	std::getline(sstream, name,		delimiter);
	std::getline(sstream, version,	delimiter);

	// represent wildcard internally as empty string
	if (vendor	== "*") vendor  = "";
	if (library	== "*") library = "";
	if (name		== "*") name    = "";
	if (version	== "*") version = "";
}

std::string
Vlnv::toString() const
{
	std::stringstream stream;
	std::string string;

	stream << *this;
	stream >> string;

	return string;
}

} // namespace fpga
} // namespace villas
