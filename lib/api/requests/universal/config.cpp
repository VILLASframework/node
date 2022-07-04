/** The Universal Data-exchange API.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

class ConfigRequest : public UniversalRequest {
public:
	using UniversalRequest::UniversalRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("This endpoint does not accept any body data");

		return new JsonResponse(session, HTTP_STATUS_OK, node->getConfig());
	}
};

/* Register API requests */
static char n[] = "universal/config";
static char r[] = "/universal/(" RE_NODE_NAME ")/config";
static char d[] = "get configuration of universal data-exchange API";
static RequestPlugin<ConfigRequest, n, r, d> p;

} /* namespace universal */
} /* namespace api */
} /* namespace node */
} /* namespace villas */
