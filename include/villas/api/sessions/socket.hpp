/** Socket Api session.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/api/session.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Api;

namespace api {
namespace sessions {

class Socket : public Session {

protected:
	int sd;

public:
	Socket(Api *a, int s);
	~Socket();

	int read();
	int write();

	virtual std::string getName();

	int getSd() const { return sd; }
};

} // sessions
} // api
} // node
} // villas
