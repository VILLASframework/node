/** Vendor, Library, Name, Version (VLNV) tag.
 *
 * @file
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <string>
#include <sstream>
#include <iostream>

namespace villas {
namespace fpga {


class Vlnv {
public:

	static constexpr char delimiter	= ':';

	Vlnv() :
	    vendor(""), library(""), name(""), version("") {}

	Vlnv(std::string s) {
		parseFromString(s);
	}

	std::string
	toString() const
	{
		std::stringstream stream;
		std::string string;

		stream << *this;
		stream >> string;

		return string;
	}

	bool
	operator==(const Vlnv& other) const;

	friend std::ostream&
	operator<< (std::ostream& stream, const Vlnv& vlnv)
	{
		return stream
		        << (vlnv.vendor.empty()		? "*" : vlnv.vendor)	<< ":"
		        << (vlnv.library.empty()	? "*" : vlnv.library)	<< ":"
		        << (vlnv.name.empty()		? "*" : vlnv.name)		<< ":"
		        << (vlnv.version.empty()	? "*" : vlnv.version);
	}

private:
	void
	parseFromString(std::string vlnv);

	std::string vendor;
	std::string library;
	std::string name;
	std::string version;
};

} // namespace fpga
} // namespace villas

/** _FPGA_VLNV_HPP_ @} */
