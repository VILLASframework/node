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

#pragma once

#include <regex>
#include <jansson.h>

#include <villas/log.hpp>
#include <villas/plugin.hpp>
#include <villas/super_node.hpp>
#include <villas/api/session.hpp>

#define RE_UUID "[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}"

namespace villas {
namespace node {
namespace api {

/* Forward declarations */
class Session;
class Response;
class RequestFactory;

class Request {

	friend RequestFactory;
	friend Response;

protected:
	Session *session;

	Logger logger;

public:
	std::string uri;
	std::smatch matches;
	json_t *body;

	RequestFactory *factory;

	enum Method {
		UNKNOWN,
		GET,
		POST,
		DELETE,
		OPTIONS,
		PUT,
		PATCH
	} method;

	Request(Session *s) :
		session(s),
		logger(logging.get("api:request")),
		body(nullptr),
		factory(nullptr),
		method(Method::UNKNOWN)
	{ }

	virtual ~Request()
	{
		if (body)
			json_decref(body);
	}

	virtual Response * execute() = 0;

	virtual void prepare()
	{ }

	void setBody(json_t *j)
	{
		body = j;
	}
};

class RequestFactory : public plugin::Plugin {

public:
	using plugin::Plugin::Plugin;

	virtual bool
	match(const std::string &uri, std::smatch &m) const = 0;

	virtual Request *
	make(Session *s) = 0;

	static Request *
	make(Session *s, const std::string &uri, Request::Method meth);
};

template<typename T, const char *name, const char *re, const char *desc>
class RequestPlugin : public RequestFactory {

protected:
	std::regex regex;

public:
	RequestPlugin() :
		RequestFactory(),
		regex(re)
	{ }

	virtual Request *
	make(Session *s)
	{
		return new T(s);
	}

	// Get plugin name
	virtual std::string
	getName() const
	{
		return name;
	}

	// Get plugin description
	virtual std::string
	getDescription() const
	{
		return desc;
	}

	virtual bool
	match(const std::string &uri, std::smatch &match) const
	{
		return std::regex_match(uri, match, regex);
	}
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
