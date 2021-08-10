/** Message related functions
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <cstdlib>

#include <villas/format.hpp>

namespace villas {
namespace node {

/* Forward declarations. */
struct Sample;

class VillasBinaryFormat : public BinaryFormat {

protected:
	uint8_t source_index;
	bool web;
	bool validate_source_index;

public:
	VillasBinaryFormat(int fl, bool w, uint8_t sid = 0) :
		BinaryFormat(fl),
		source_index(sid),
		web(w),
		validate_source_index(false)
	{ }

	virtual
	int sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt);
	virtual
	int sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt);

	virtual
	void parse(json_t *json);
};


template<bool web = false>
class VillasBinaryFormatPlugin : public FormatFactory {

public:
	using FormatFactory::FormatFactory;

	virtual
	Format * make()
	{
		return new VillasBinaryFormat((int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA, web);
	}

	/// Get plugin name
	virtual
	std::string getName() const
	{
		std::stringstream ss;

		ss << "villas." << (web ? "web" : "binary");

		return ss.str();
	}

	/// Get plugin description
	virtual
	std::string getDescription() const
	{
		std::stringstream ss;

		ss << "VILLAS binary network format";

		if (web)
			ss << " for WebSockets";

		return ss.str();
	}
};

} /* namespace node */
} /* namespace villas */
