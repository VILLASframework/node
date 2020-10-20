/** API Request.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

std::string
Request::toString()
{
	return fmt::format("endpoint={}, method={}", factory->getName(), Session::methodToString(method));
}

Request * RequestFactory::create(Session *s, const std::string &uri, Session::Method meth, unsigned long ct)
{
	info("api: Trying to find request handler for: uri=%s", uri.c_str());

	for (auto *rf : plugin::Registry::lookup<RequestFactory>()) {
		std::smatch mr;
		if (not rf->match(uri, mr))
			continue;

		auto *p = rf->make(s);

		p->matches = mr;
		p->factory = rf;
		p->method = meth;
		p->contentLength = ct;

		return p;
	}

	throw BadRequest("Unknown API request", "{ s: s, s: s }",
		"uri", uri.c_str(),
		"method", Session::methodToString(meth).c_str()
	);
}
