/** The Universal Data-exchange API.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/api/requests/universal.hpp>
#include <villas/api/response.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

class SignalRequest : public UniversalRequest {
public:
	using UniversalRequest::UniversalRequest;

	Response * executeGet(const std::string &signalName)
	{
		if (body != nullptr)
			throw BadRequest("This endpoint does not accept any body data");

		pthread_mutex_lock(&api_node->write.mutex);

		auto *smp = api_node->write.sample;
		if (!smp) {
			pthread_mutex_unlock(&api_node->write.mutex);
			throw Error(HTTP_STATUS_NOT_FOUND, "No data available");
		}

		auto idx = smp->signals->getIndexByName(signalName);
		if (idx < 0) {
			pthread_mutex_unlock(&api_node->write.mutex);
			throw Error(HTTP_STATUS_NOT_FOUND, "Unknown signal id: {}", signalName);
		}

		auto sig = smp->signals->getByIndex(idx);
		auto *json_signal = json_pack("{ s: f, s: o }",
			"timestamp", time_to_double(&smp->ts.origin),
			"value", smp->data[idx].toJson(sig->type)
		);

		if (smp->length <= (unsigned) idx)
			smp->length = idx + 1;

		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_DATA;

		pthread_mutex_unlock(&api_node->write.mutex);

		return new JsonResponse(session, HTTP_STATUS_OK, json_signal);
	}

	Response * executePut(const std::string &signalName)
	{
		int ret;

		pthread_mutex_lock(&api_node->read.mutex);

		auto *smp = api_node->read.sample;
		if (!smp) {
			pthread_mutex_unlock(&api_node->read.mutex);
			throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, "Not initialized yet");
		}

		auto idx = smp->signals->getIndexByName(signalName);
		if (idx < 0) {
			pthread_mutex_unlock(&api_node->read.mutex);
			throw BadRequest("Unknown signal id: {}", signalName);
		}

		auto sig = smp->signals->getByIndex(idx);

		double timestamp = 0;
		double value = 0;

		json_error_t err;
		ret = json_unpack_ex(body, &err, 0, "{ s: F, s: F }",
			"timestamp", &timestamp,
			"value", &value
		);
		if (ret) {
			pthread_mutex_unlock(&api_node->read.mutex);
			throw BadRequest("Malformed body: {}", err.text);
		}

		smp->ts.origin = time_from_double(timestamp);
		smp->data[idx].f = value;

		pthread_cond_signal(&api_node->read.cv);
		pthread_mutex_unlock(&api_node->read.mutex);

		return new JsonResponse(session, HTTP_STATUS_OK, json_object());
	}

	virtual Response * execute()
	{
		auto const &signalName = matches[2];

		switch (method) {
		case Session::Method::GET:
			return executeGet(signalName);
		case Session::Method::PUT:
			return executePut(signalName);
		default:
			throw InvalidMethod(this);
		}
	}
};

/* Register API requests */
static char n[] = "universal/signal";
static char r[] = "/universal/(" RE_NODE_NAME ")/signal/([a-z0-9_-]+)/state";
static char d[] = "get or set signal of universal data-exchange API";
static RequestPlugin<SignalRequest, n, r, d> p;

} /* namespace universal */
} /* namespace api */
} /* namespace node */
} /* namespace villas */
