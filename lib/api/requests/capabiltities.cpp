/** The "capabiltities" API ressource.
 *
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

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>
#include <villas/capabilities.hpp>

namespace villas {
namespace node {
namespace api {

class CapabilitiesRequest : public Request {

public:
	using Request::Request;

	virtual Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Capabilities endpoint does not accept any body data");

		auto *json_capabilities = getCapabilities();

		return new JsonResponse(session, HTTP_STATUS_OK, json_capabilities);
	}
};

/* Register API request */
static char n[] = "capabilities";
static char r[] = "/capabilities";
static char d[] = "get capabiltities and details about this VILLASnode instance";
static RequestPlugin<CapabilitiesRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
