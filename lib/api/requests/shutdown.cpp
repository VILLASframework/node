/** The "shutdown" API request.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <signal.h>

#include <villas/utils.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class ShutdownRequest : public Request {

public:
	using Request::Request;

	virtual Response * execute()
	{
		if (method != Session::Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Shutdown endpoint does not accept any body data");

		utils::killme(SIGTERM);

		return new Response(session, HTTP_STATUS_OK);
	}
};

/* Register API request */
static char n[] = "shutdown";
static char r[] = "/shutdown";
static char d[] = "quit VILLASnode";
static RequestPlugin<ShutdownRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
