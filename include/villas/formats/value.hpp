/** The VILLASframework sample format
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdio>

#include <villas/format.hpp>

namespace villas {
namespace node {

class ValueFormat : public Format {

public:
	using Format::Format;

	virtual
	int sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt);
	virtual
	int sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt);
};

} /* namespace node */
} /* namespace villas */
