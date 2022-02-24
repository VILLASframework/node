/** A custom format for OPAL-RTs AsyncIP example
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

class OpalAsyncIPFormat : public BinaryFormat {

protected:
	const int MAXSIZE = 64;

	struct Payload {
		int16_t dev_id;		// (2 bytes) Sender device ID
		int32_t msg_id;		// (4 bytes) Message ID
		int16_t msg_len;	// (2 bytes) Message length (data only)
		double  data[]; 	// Up to MAXSIZE doubles (8 bytes each)
	} __attribute__((packed));

	int16_t dev_id;

public:
	OpalAsyncIPFormat(int fl, uint8_t did = 0) :
		BinaryFormat(fl),
		dev_id(did)
	{ }

	virtual
	int sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt);
	virtual
	int sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt);

	virtual
	void parse(json_t *json);
};


class OpalAsyncIPFormatPlugin : public FormatFactory {

public:
	using FormatFactory::FormatFactory;

	virtual
	Format * make()
	{
		return new OpalAsyncIPFormat((int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA);
	}

	/// Get plugin name
	virtual
	std::string getName() const
	{
		return "opal.asyncip";
	}

	/// Get plugin description
	virtual
	std::string getDescription() const
	{
		return "OPAL-RTs AsyncIP example format";
	}
};

} /* namespace node */
} /* namespace villas */
