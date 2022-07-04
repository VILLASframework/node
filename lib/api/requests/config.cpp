/** The "config" API ressource.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/super_node.hpp>
#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class ConfigRequest : public Request {

public:
	using Request::Request;

	virtual Response * execute()
	{
		json_t *json = session->getSuperNode()->getConfig();

		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Config endpoint does not accept any body data");

		auto *json_config = json
			? json_incref(json)
			: json_object();

		return new JsonResponse(session, HTTP_STATUS_OK, json_config);
	}
};

/* Register API request */
static char n[] = "config";
static char r[] = "/config";
static char d[] = "get configuration of this VILLASnode instance";
static RequestPlugin<ConfigRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
