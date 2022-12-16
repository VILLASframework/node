/** The "shutdown" API request.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
