/** API Request.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/plugin.hpp>
#include <villas/api.hpp>
#include <villas/api/request.hpp>

using namespace villas;
using namespace villas::node::api;

void Request::decode()
{
	body = buffer.decode();
	if (!body)
		throw BadRequest("Failed to decode request payload");
}

std::string Request::toString()
{
	return fmt::format("endpoint={}, method={}", factory->getName(), Session::methodToString(method));
}

Request * RequestFactory::create(Session *s, const std::string &uri, Session::Method meth, unsigned long ct)
{
	s->logger->info("Lookup request handler for: uri={}", uri);

	for (auto *rf : plugin::registry->lookup<RequestFactory>()) {
		std::smatch mr;
		if (not rf->match(uri, mr))
			continue;

		auto *p = rf->make(s);

		for (auto m : mr)
			p->matches.push_back(m.str());

		p->factory = rf;
		p->method = meth;
		p->contentLength = ct;

		p->prepare();

		return p;
	}

	throw BadRequest("Unknown API request", "{ s: s, s: s }",
		"uri", uri.c_str(),
		"method", Session::methodToString(meth).c_str()
	);
}
