/** The Universal Data-exchange API.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/api/requests/universal.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

class SignalsRequest : public UniversalRequest {
public:
	using UniversalRequest::UniversalRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("This endpoint does not accept any body data");

		auto *json_sigs = json_array();
		for (auto const &sig : *node->getOutputSignals()) {
			auto *json_sig = json_pack("{ s: s, s: s, s: b, s: b, s: o }",
				"id", fmt::format("out/{}", sig->name).c_str(),
				"source", node->getNameShort().c_str(),
				"readable", true,
				"writable", false,
				"value", json_null()
			);

			json_array_append(json_sigs, json_sig);
		}

		for (auto const &sig : *node->getInputSignals()) {
			auto *json_sig = json_pack("{ s: s, s: s, s: b, s: b, s: o }",
				"id", fmt::format("in/{}", sig->name).c_str(),
				"source", node->getNameShort().c_str(),
				"readable", false,
				"writable", true,
				"value", json_null()
			);

			json_array_append(json_sigs, json_sig);
		}

		return new JsonResponse(session, HTTP_STATUS_OK, json_sigs);
	}
};

/* Register API requests */
static char n[] = "universal/signals";
static char r[] = "/universal/(" RE_NODE_NAME ")/signals";
static char d[] = "get signals of universal data-exchange API node";
static RequestPlugin<SignalsRequest, n, r, d> p;

} /* namespace universal */
} /* namespace api */
} /* namespace node */
} /* namespace villas */
