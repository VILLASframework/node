/** The Universal Data-exchange API.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/api/requests/universal.hpp>
#include <villas/api/response.hpp>
#include <villas/node.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

class StatusRequest : public UniversalRequest {
public:
	using UniversalRequest::UniversalRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("This endpoint does not accept any body data");

		auto *json_response = json_pack("{ s: s }",
			// TODO: Add connectivity check or heuristic here.
			"connected", "unknown"
		);

		return new JsonResponse(session, HTTP_STATUS_OK, json_response);
	}
};

/* Register API requests */
static char n[] = "universal/status";
static char r[] = "/universal/(" RE_NODE_NAME ")/status";
static char d[] = "get status of universal data-exchange API";
static RequestPlugin<StatusRequest, n, r, d> p;

} // namespace universal
} // namespace api
} // namespace node
} // namespace villas
