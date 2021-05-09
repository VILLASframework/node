/** The VILLASframework sample format
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/formats/line.hpp>

namespace villas {
namespace node {

class VILLASHumanFormat : public LineFormat {

protected:
	size_t sprintLine(char *buf, size_t len, const struct sample *smp);
	size_t sscanLine(const char *buf, size_t len, struct sample *smp);

public:
	using LineFormat::LineFormat;

	void header(FILE *f, const struct vlist *sigs);
};

} /* namespace node */
} /* namespace villas */
