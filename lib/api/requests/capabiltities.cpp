/** The "capabiltities" API ressource.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
